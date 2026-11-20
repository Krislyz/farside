// Farside pairing and controller link: the parts that hold no sockets and no processes.
// Ported from relay/Link.cs (the C# app this replaces); the two must agree on every format here,
// because a Host and a Client may run different builds:
//   pairing code   farside:1:<ticket>:<32 hex>
//   proof          HMAC-SHA256 over the nonce bytes, lower-case hex
//   key store      key=value lines (id, secret, host, peer), encrypted for the current Windows user
// The wire protocol table is in docs/superpowers/plans/2026-11-19-ps5link-pairing-and-client.md, Task 2.
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace Farside {

// A PC that was paired with this Host.
struct Peer
{
	QString id, name;
	QByteArray key;
};

QString makeCode(const QString &ticket, const QByteArray &secret);
// Accepts surrounding whitespace and capitals (the QR code carries the text in capitals).
bool parseCode(const QString &text, QString *ticket, QByteArray *secret);
// Proof that the sender knows the pairing key.
QString proof(const QByteArray &key, const QString &nonce_hex);
// Compares in time that does not depend on where the texts differ.
bool sameText(const QString &a, const QString &b);
// Splits a protocol line on single spaces, at most 6 parts.
QStringList splitLine(const QString &line);
// Names travel as one word in a protocol line and as one field in the store.
QString cleanName(const QString &name);
// The ticket is the last word dumbpipe prints after "To connect, use e.g.:". Empty if there is none.
QString parseTicket(const QString &stderr_text);
QByteArray randomBytes(int count);

// ---- Direct connections: Host and Client talk over the network they share (home network, VPN) instead
// of through the tunnel. Nothing protects such a connection from others on that network, so pairing
// must not send a secret over it. The two PCs agree on a key instead (X25519): each sends a public
// value, both work out the same secret from it, and an eavesdropper cannot. To rule out someone in the
// middle answering for the other PC, both show a number made from that secret; the user compares.
bool agreeKeys(QByteArray *secret_key, QByteArray *public_key);                        // 32 bytes each
QByteArray agreeShared(const QByteArray &secret_key, const QByteArray &their_public);  // 32 bytes; empty if it failed
QString checkNumber(const QByteArray &shared, const QString &nonce_hex);               // 4 digits, the same on both PCs
QByteArray directKey(const QByteArray &shared, const QString &nonce_hex);              // the pairing key, 32 bytes
// Where a directly paired Host is: kept in the place of the tunnel ticket as direct:<host>:<port>.
QString makeDirect(const QString &host, int port);
bool parseDirect(const QString &ticket, QString *host, int *port);
// Finding Hosts on this network: the Client broadcasts the question, a Host answers the asker.
QByteArray lookupQuestion();
bool isLookupQuestion(const QByteArray &datagram);
QByteArray lookupAnswer(const QString &id, const QString &name, int port, bool pairing_open);
bool parseLookupAnswer(const QByteArray &datagram, QString *id, QString *name, int *port, bool *pairing_open);

// This PC's identity and its pairings. One file per user.
class Store
{
	public:
		// %APPDATA%\Farside\link.dat on Windows (the same file the C# app used, so pairings carry over),
		// ~/.config/Farside/link.dat elsewhere.
		static QString defaultPath();

		QString path = defaultPath();
		QString my_id;       // 16 hex chars, made once
		QString iroh_secret; // 64 hex chars, made once; gives dumbpipe the same address after a restart
		// Client: the Host this PC is paired with. host_ticket is empty when not paired.
		QString host_ticket, host_name;
		QByteArray host_key;
		// Host: the Clients paired with this PC.
		QList<Peer> peers;

		// A file that cannot be read counts as empty and is left on disk until the next save.
		void load();
		bool save();

	private:
		bool unreadable = false;
};

// 0 = pass, otherwise the number of the first failing check. Run: chiaki --farside-selftest
int selfTest();

} // namespace Farside
