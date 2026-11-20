// See farsidelink.h.
#include <openssl/evp.h>
#include <QMessageAuthenticationCode>
#include <farsidelink.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace Farside {

QString makeCode(const QString &ticket, const QByteArray &secret)
{
	return QStringLiteral("farside:1:%1:%2").arg(ticket, QString::fromLatin1(secret.toHex()));
}

bool parseCode(const QString &text, QString *ticket, QByteArray *secret)
{
	static const QRegularExpression shape(QStringLiteral("^farside:1:([a-z0-9]+):([0-9a-f]{32})$"));
	const QRegularExpressionMatch m = shape.match(text.trimmed().toLower());
	if(!m.hasMatch())
		return false;
	*ticket = m.captured(1);
	*secret = QByteArray::fromHex(m.captured(2).toLatin1());
	return true;
}

QString proof(const QByteArray &key, const QString &nonce_hex)
{
	return QString::fromLatin1(QMessageAuthenticationCode::hash(QByteArray::fromHex(nonce_hex.toLatin1()), key, QCryptographicHash::Sha256).toHex());
}

bool sameText(const QString &a, const QString &b)
{
	int diff = a.size() ^ b.size();
	for(int i = 0; i < a.size() && i < b.size(); i++)
		diff |= a[i].unicode() ^ b[i].unicode();
	return diff == 0;
}

QStringList splitLine(const QString &line)
{
	// Like String.Split(' ', 6): the sixth part keeps whatever is left, spaces included.
	QStringList parts;
	int from = 0;
	while(parts.size() < 5)
	{
		const int space = line.indexOf(QLatin1Char(' '), from);
		if(space < 0)
			break;
		parts.append(line.mid(from, space - from));
		from = space + 1;
	}
	parts.append(line.mid(from));
	return parts;
}

QString cleanName(const QString &name)
{
	static const QRegularExpression other(QStringLiteral("[^A-Za-z0-9._-]"));
	QString s = name;
	s.replace(other, QStringLiteral("_"));
	return s.isEmpty() ? QStringLiteral("PC") : s.left(40);
}

QString parseTicket(const QString &stderr_text)
{
	const int at = stderr_text.indexOf(QStringLiteral("To connect, use e.g.:"));
	if(at < 0)
		return QString();
	static const QRegularExpression space(QStringLiteral("\\s+"));
	static const QRegularExpression shape(QStringLiteral("^[A-Za-z0-9]{20,}$"));
	const QStringList words = stderr_text.mid(at).split(space, Qt::SkipEmptyParts);
	return shape.match(words.last()).hasMatch() ? words.last() : QString();
}

bool agreeKeys(QByteArray *secret_key, QByteArray *public_key)
{
	EVP_PKEY *pair = EVP_PKEY_Q_keygen(nullptr, nullptr, "X25519");
	if(!pair)
		return false;
	size_t a = 32, b = 32;
	secret_key->resize(32);
	public_key->resize(32);
	const bool ok = EVP_PKEY_get_raw_private_key(pair, reinterpret_cast<unsigned char *>(secret_key->data()), &a) == 1
		&& EVP_PKEY_get_raw_public_key(pair, reinterpret_cast<unsigned char *>(public_key->data()), &b) == 1 && a == 32 && b == 32;
	EVP_PKEY_free(pair);
	return ok;
}

QByteArray agreeShared(const QByteArray &secret_key, const QByteArray &their_public)
{
	if(secret_key.size() != 32 || their_public.size() != 32)
		return {};
	EVP_PKEY *mine = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, reinterpret_cast<const unsigned char *>(secret_key.constData()), 32);
	EVP_PKEY *theirs = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, reinterpret_cast<const unsigned char *>(their_public.constData()), 32);
	EVP_PKEY_CTX *ctx = mine && theirs ? EVP_PKEY_CTX_new(mine, nullptr) : nullptr;
	QByteArray shared(32, 0);
	size_t len = 32;
	const bool ok = ctx && EVP_PKEY_derive_init(ctx) == 1 && EVP_PKEY_derive_set_peer(ctx, theirs) == 1
		&& EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char *>(shared.data()), &len) == 1 && len == 32;
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(mine);
	EVP_PKEY_free(theirs);
	return ok ? shared : QByteArray();
}

QString checkNumber(const QByteArray &shared, const QString &nonce_hex)
{
	const QByteArray mac = QMessageAuthenticationCode::hash("farside check " + nonce_hex.toLatin1(), shared, QCryptographicHash::Sha256);
	const quint32 n = (quint32(quint8(mac[0])) << 24) | (quint32(quint8(mac[1])) << 16) | (quint32(quint8(mac[2])) << 8) | quint32(quint8(mac[3]));
	return QStringLiteral("%1").arg(n % 10000, 4, 10, QLatin1Char('0'));
}

QByteArray directKey(const QByteArray &shared, const QString &nonce_hex)
{
	return QMessageAuthenticationCode::hash("farside key " + nonce_hex.toLatin1(), shared, QCryptographicHash::Sha256);
}

QString makeDirect(const QString &host, int port)
{
	return QStringLiteral("direct:%1:%2").arg(host).arg(port);
}

bool parseDirect(const QString &ticket, QString *host, int *port)
{
	if(!ticket.startsWith(QLatin1String("direct:")))
		return false;
	const int colon = ticket.lastIndexOf(QLatin1Char(':'));
	bool ok = false;
	const int number = ticket.mid(colon + 1).toInt(&ok);
	if(colon <= 7 || !ok || number <= 0 || number > 65535)
		return false;
	if(host) *host = ticket.mid(7, colon - 7);
	if(port) *port = number;
	return true;
}

QByteArray lookupQuestion() { return QByteArrayLiteral("FARSIDE? 1"); }
bool isLookupQuestion(const QByteArray &datagram) { return datagram == lookupQuestion(); }

QByteArray lookupAnswer(const QString &id, const QString &name, int port, bool pairing_open)
{
	return QStringLiteral("FARSIDE! 1 %1 %2 %3 %4").arg(id, cleanName(name)).arg(port).arg(pairing_open ? 1 : 0).toUtf8();
}

bool parseLookupAnswer(const QByteArray &datagram, QString *id, QString *name, int *port, bool *pairing_open)
{
	const QStringList f = QString::fromUtf8(datagram).split(QLatin1Char(' '));
	bool ok = false;
	const int number = f.size() == 6 ? f[4].toInt(&ok) : 0;
	if(f.size() != 6 || f[0] != QLatin1String("FARSIDE!") || f[1] != QLatin1String("1") || f[2].size() != 16 || !ok || number <= 0 || number > 65535)
		return false;
	if(id) *id = f[2];
	if(name) *name = f[3];
	if(port) *port = number;
	if(pairing_open) *pairing_open = f[5] == QLatin1String("1");
	return true;
}

QByteArray randomBytes(int count)
{
	QByteArray b(count, Qt::Uninitialized);
	// The system generator is the operating system's cryptographic one.
	for(int i = 0; i < count; i++)
		b[i] = char(QRandomGenerator::system()->bounded(256));
	return b;
}

// ---- key store ----

// Windows: encrypted for the current user (DPAPI), as the C# app did. Elsewhere the file's own
// permissions (owner only, set in Store::save) are the protection.
static QByteArray protect(const QByteArray &plain)
{
#ifdef Q_OS_WIN
	DATA_BLOB in = { DWORD(plain.size()), reinterpret_cast<BYTE *>(const_cast<char *>(plain.data())) };
	DATA_BLOB out = {};
	if(!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out))
		return QByteArray();
	const QByteArray r(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
	LocalFree(out.pbData);
	return r;
#else
	return plain;
#endif
}

static bool unprotect(const QByteArray &stored, QByteArray *plain)
{
#ifdef Q_OS_WIN
	DATA_BLOB in = { DWORD(stored.size()), reinterpret_cast<BYTE *>(const_cast<char *>(stored.data())) };
	DATA_BLOB out = {};
	if(!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out))
		return false;
	*plain = QByteArray(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
	LocalFree(out.pbData);
	return true;
#else
	*plain = stored;
	return true;
#endif
}

QString Store::defaultPath()
{
#ifdef Q_OS_WIN
	return QDir(qEnvironmentVariable("APPDATA")).filePath(QStringLiteral("Farside/link.dat"));
#else
	return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath(QStringLiteral("Farside/link.dat"));
#endif
}

void Store::load()
{
	static const QRegularExpression id_shape(QStringLiteral("^[0-9a-f]{16}$"));
	static const QRegularExpression secret_shape(QStringLiteral("^[0-9a-f]{64}$"));
	my_id.clear(); iroh_secret.clear(); host_ticket.clear(); host_name.clear(); host_key.clear();
	peers.clear();
	unreadable = false;

	QFile file(path);
	if(file.exists())
	{
		QByteArray plain;
		if(!file.open(QIODevice::ReadOnly) || !unprotect(file.readAll(), &plain))
			unreadable = true;
		else
		{
			for(const QString &line : QString::fromUtf8(plain).split(QLatin1Char('\n')))
			{
				const int eq = line.indexOf(QLatin1Char('='));
				if(eq < 0)
					continue;
				const QString k = line.left(eq), v = line.mid(eq + 1).trimmed();
				const QStringList f = v.split(QLatin1Char('|'));
				if(k == QLatin1String("id") && id_shape.match(v).hasMatch())
					my_id = v;
				else if(k == QLatin1String("secret") && secret_shape.match(v).hasMatch())
					iroh_secret = v;
				else if(k == QLatin1String("host") && f.size() == 3)
				{
					host_ticket = f[0]; host_name = f[1]; host_key = QByteArray::fromHex(f[2].toLatin1());
				}
				else if(k == QLatin1String("peer") && f.size() == 3)
					peers.append({ f[0], f[1], QByteArray::fromHex(f[2].toLatin1()) });
			}
		}
	}
	const bool fresh = my_id.isEmpty() || iroh_secret.isEmpty();
	if(my_id.isEmpty())
		my_id = QString::fromLatin1(randomBytes(8).toHex());
	if(iroh_secret.isEmpty())
		iroh_secret = QString::fromLatin1(randomBytes(32).toHex());
	if(fresh && !unreadable)
		save();
}

bool Store::save()
{
	QString text = QStringLiteral("id=%1\nsecret=%2\n").arg(my_id, iroh_secret);
	if(!host_ticket.isEmpty())
		text += QStringLiteral("host=%1|%2|%3\n").arg(host_ticket, cleanName(host_name), QString::fromLatin1(host_key.toHex()));
	for(const Peer &p : peers)
		text += QStringLiteral("peer=%1|%2|%3\n").arg(p.id, cleanName(p.name), QString::fromLatin1(p.key.toHex()));
	const QByteArray stored = protect(text.toUtf8());
	if(stored.isEmpty())
		return false;
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
	const bool ok = file.write(stored) == stored.size();
	file.close();
	if(ok)
		unreadable = false;
	return ok;
}

// ---- self-test: the same checks, with the same numbers, as Link.SelfTest in relay/Link.cs ----

int selfTest()
{
	QByteArray s(16, 0);
	for(int i = 0; i < 16; i++)
		s[i] = char(i);
	const QString code = makeCode(QStringLiteral("endpointabc234"), s);
	QString t;
	QByteArray back;
	if(code != QLatin1String("farside:1:endpointabc234:000102030405060708090a0b0c0d0e0f")) return 1;
	if(!parseCode(QStringLiteral("  ") + code + QStringLiteral("\r\n"), &t, &back) || t != QLatin1String("endpointabc234") || back.size() != 16 || back[15] != 15) return 2;
	if(parseCode(QStringLiteral("farside:2:endpointabc234:000102030405060708090a0b0c0d0e0f"), &t, &back)) return 3;
	if(parseCode(QStringLiteral("farside:1:endpointabc234:0001"), &t, &back)) return 4;
	if(parseCode(QStringLiteral("hello"), &t, &back) || parseCode(QString(), &t, &back)) return 5;
	// RFC 4231 test case 2: key "Jefe", data "what do ya want for nothing?"
	if(proof(QByteArrayLiteral("Jefe"), QStringLiteral("7768617420646f2079612077616e7420666f72206e6f7468696e673f"))
			!= QLatin1String("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843")) return 6;
	if(!sameText(QStringLiteral("abc"), QStringLiteral("abc")) || sameText(QStringLiteral("abc"), QStringLiteral("abd")) || sameText(QStringLiteral("abc"), QStringLiteral("ab"))) return 7;
	if(splitLine(QStringLiteral("AUTH id proof")).size() != 3 || splitLine(QString()).size() != 1) return 8;

	QTemporaryDir dir;
	Store store;
	store.path = dir.filePath(QStringLiteral("link.dat"));
	store.load();
	if(store.my_id.size() != 16 || !store.peers.isEmpty()) return 9;
	const QString id = store.my_id;
	const QByteArray k0 = QByteArray::fromHex(QByteArray(64, '1'));
	store.peers.append({ QStringLiteral("00112233aabbccdd"), QStringLiteral("WorkPC-Name"), k0 });
	if(!store.save()) return 10;
	store.load();
	if(store.my_id != id || store.peers.size() != 1 || store.peers[0].id != QLatin1String("00112233aabbccdd")
			|| store.peers[0].name != QLatin1String("WorkPC-Name") || store.peers[0].key != k0) return 10;
#ifdef Q_OS_WIN
	{
		QFile raw(store.path);
		if(!raw.open(QIODevice::ReadOnly) || raw.readAll().contains("WorkPC-Name")) return 11;
	}
#endif
	{
		QFile broken(store.path);
		if(!broken.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 12;
		broken.write(randomBytes(200));
	}
	store.load();
#ifdef Q_OS_WIN
	if(!store.peers.isEmpty() || store.my_id.size() != 16) return 12;
#else
	if(store.my_id.size() != 16) return 12;
#endif

	if(parseTicket(QStringLiteral("using secret key x\nForwarding incoming requests to '127.0.0.1:1'.\nTo connect, use e.g.:\ndumbpipe connect-tcp endpointabc234endpointabc234\n"))
			!= QLatin1String("endpointabc234endpointabc234")) return 13;
	if(!parseTicket(QStringLiteral("error: boom")).isEmpty() || !parseTicket(QString()).isEmpty()) return 14;
	return 0;
}

} // namespace Farside
