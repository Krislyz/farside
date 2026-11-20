// See farsidesession.h.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTemporaryDir>
#include <farsidesession.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>

#include <atomic>
#include <memory>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Farside {

static const QRegularExpression bus_shape(QStringLiteral("^\\d+-[\\d.]+$"));
static const QRegularExpression id_shape(QStringLiteral("^[0-9a-f]{16}$"));
static const QRegularExpression key_shape(QStringLiteral("^[0-9a-f]{64}$"));

static QString hex(const QByteArray &b) { return QString::fromLatin1(b.toHex()); }
static QString myName() { return cleanName(QSysInfo::machineHostName()); }

static void send(QTcpSocket *s, const QString &line)
{
	if(s && s->state() == QAbstractSocket::ConnectedState)
		s->write((line + QLatin1Char('\n')).toUtf8());
}

// Takes one protocol line if a whole one has arrived. A line that is too long closes the connection.
// Reads no further than the newline: raw USB/IP bytes may follow a DATA line.
static bool takeLine(QTcpSocket *s, QString *line)
{
	if(!s->canReadLine())
	{
		if(s->bytesAvailable() > 512)
			s->abort();
		return false;
	}
	const QByteArray raw = s->readLine(514);
	if(!raw.endsWith('\n'))
	{
		s->abort();
		return false;
	}
	*line = QString::fromUtf8(raw).trimmed();
	return true;
}

// Joins two connections until either side ends. Whatever one side has already sent goes across first.
// ponytail: no limit on what piles up for a slow side; USB/IP traffic of one controller is tiny.
static void bridge(QTcpSocket *a, QTcpSocket *b)
{
	a->disconnect();
	b->disconnect();
	auto pump = [](QTcpSocket *from, QTcpSocket *to) {
		from->setSocketOption(QAbstractSocket::LowDelayOption, 1);
		QObject::connect(from, &QTcpSocket::readyRead, to, [from, to] { to->write(from->readAll()); });
		QObject::connect(from, &QTcpSocket::disconnected, to, [to] { to->disconnectFromHost(); });
		QObject::connect(from, &QTcpSocket::disconnected, from, &QObject::deleteLater);
		if(from->bytesAvailable())
			to->write(from->readAll());
	};
	pump(a, b);
	pump(b, a);
	if(a->state() != QAbstractSocket::ConnectedState)
		b->disconnectFromHost();
	if(b->state() != QAbstractSocket::ConnectedState)
		a->disconnectFromHost();
}

// A console program started from this windowed program would flash a console window; this stops that.
static void noWindow(QProcess *p)
{
#ifdef Q_OS_WIN
	p->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) { a->flags |= CREATE_NO_WINDOW; });
#else
	Q_UNUSED(p);
#endif
}

// A helper program: next to this program first, then in the folder it installs to, then on PATH.
QString findTool(const QString &name, const QString &program_files_dir)
{
#ifdef Q_OS_WIN
	const QString file = name + QStringLiteral(".exe");
#else
	const QString file = name;
#endif
	const QString beside = QDir(QCoreApplication::applicationDirPath()).filePath(file);
	if(QFile::exists(beside))
		return beside;
	if(!program_files_dir.isEmpty())
	{
		const QString installed = QDir(qEnvironmentVariable("ProgramFiles")).filePath(program_files_dir + QLatin1Char('/') + file);
		if(QFile::exists(installed))
			return installed;
	}
	return QStandardPaths::findExecutable(name);
}

void runTool(QObject *owner, const QString &exe, const QStringList &args, std::function<void(int, QString)> done)
{
	if(exe.isEmpty())
	{
		done(-1, QString());
		return;
	}
	auto *p = new QProcess(owner);
	noWindow(p);
	p->setProcessChannelMode(QProcess::MergedChannels);
	auto reported = std::make_shared<bool>(false);
	QObject::connect(p, &QProcess::finished, owner, [p, done, reported](int code, QProcess::ExitStatus) {
		if(*reported)
			return;
		*reported = true;
		const QString out = QString::fromLocal8Bit(p->readAll()).trimmed();
		p->deleteLater();
		done(code, out);
	});
	QObject::connect(p, &QProcess::errorOccurred, owner, [p, done, reported](QProcess::ProcessError e) {
		if(e != QProcess::FailedToStart || *reported)
			return;
		*reported = true;
		p->deleteLater();
		done(-1, QString());
	});
	p->start(exe, args);
}

// ---- Tunnel ----

Tunnel::Tunnel(QObject *parent) : QObject(parent) {}
Tunnel::~Tunnel() { stop(); }

bool Tunnel::alive() const { return proc && proc->state() != QProcess::NotRunning; }

void Tunnel::stop()
{
	if(!proc)
		return;
	proc->disconnect();
	proc->kill();
	proc->waitForFinished(1000);
	proc->deleteLater();
	proc = nullptr;
	target_.clear();
}

void Tunnel::start(const QStringList &args, const QString &iroh_secret)
{
	stop();
	err.clear();
	ticket_.clear();
	const QString exe = findTool(QStringLiteral("dumbpipe"));
	if(exe.isEmpty())
	{
		emit failed(QStringLiteral("dumbpipe is missing. Put it next to this program."));
		return;
	}
	proc = new QProcess(this);
	noWindow(proc);
	// Each PC keeps its own iroh identity, so its address stays the same across restarts.
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert(QStringLiteral("IROH_SECRET"), iroh_secret);
	proc->setProcessEnvironment(env);
	connect(proc, &QProcess::readyReadStandardError, this, [this] {
		err += QString::fromUtf8(proc->readAllStandardError());
		// -v prints the long ticket, then "or:" and the short one. Wait for the short one.
		static const QRegularExpression both(QStringLiteral("\\nor:\\s*\\n\\s*dumbpipe connect-tcp \\S+\\s*\\n"));
		if(listening && ticket_.isEmpty() && both.match(err).hasMatch())
		{
			ticket_ = parseTicket(err);
			emit ready(ticket_);
		}
	});
	connect(proc, &QProcess::finished, this, [this] {
		if(!listening || !ticket_.isEmpty())
			return;
		// dumbpipe prints a freshly generated secret key on stderr; never let that line reach the screen.
		QString last;
		for(const QString &line : err.split(QLatin1Char('\n')))
			if(!line.trimmed().isEmpty() && !line.contains(QStringLiteral("secret"), Qt::CaseInsensitive))
				last = line.trimmed();
		emit failed(QStringLiteral("The tunnel did not start. ") + last);
	});
	proc->start(exe, args);
#ifdef Q_OS_WIN
	// A dumbpipe that outlives this program would keep answering with this PC's identity. Windows ends
	// every process of this job when the last handle to the job closes, which is when this program ends,
	// however it ends.
	static const HANDLE job = [] {
		const HANDLE j = CreateJobObjectW(nullptr, nullptr);
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if(j)
			SetInformationJobObject(j, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
		return j;
	}();
	if(job && proc->waitForStarted(3000))
		if(const HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, DWORD(proc->processId())))
		{
			AssignProcessToJobObject(job, child);
			CloseHandle(child);
		}
#endif
}

void Tunnel::listen(int local_port, const QString &iroh_secret)
{
	listening = true;
	// -v also prints the short ticket (endpoint ID and relay only). The default one embeds the current
	// direct addresses and changes on every start; the short one does not.
	start({ QStringLiteral("listen-tcp"), QStringLiteral("-v"), QStringLiteral("--host"), QStringLiteral("127.0.0.1:%1").arg(local_port) }, iroh_secret);
}

void Tunnel::connectTo(int local_port, const QString &ticket, const QString &iroh_secret)
{
	listening = false;
	start({ QStringLiteral("connect-tcp"), QStringLiteral("--addr"), QStringLiteral("127.0.0.1:%1").arg(local_port), ticket }, iroh_secret);
	target_ = ticket;
}

// ---- Host ----

Host::Host(QObject *parent) : QObject(parent), tunnel(this)
{
	connect(&host_server, &QTcpServer::newConnection, this, &Host::incoming);
	connect(&lan_server, &QTcpServer::newConnection, this, &Host::incoming);
	connect(&lookup_socket, &QUdpSocket::readyRead, this, &Host::lookupIncoming);
	connect(&usb_server, &QTcpServer::newConnection, this, &Host::usbIncoming);
	connect(&ticker, &QTimer::timeout, this, &Host::tick);
	// The Client sends the number back as PONG; the difference is the round trip.
	pinger.setInterval(5000);
	connect(&pinger, &QTimer::timeout, this, [this] { send(control, QStringLiteral("PING %1").arg(QDateTime::currentMSecsSinceEpoch() % 2000000000)); });
	silence.setSingleShot(true);
	silence.setInterval(15000);
	connect(&silence, &QTimer::timeout, this, [this] { if(control) control->abort(); });
	ask_timeout.setSingleShot(true);
	ask_timeout.setInterval(60000);
	connect(&ask_timeout, &QTimer::timeout, this, [this] { answerPair(false); });
	connect(&tunnel, &Tunnel::ready, this, [this](const QString &t) { ticket = t; tunnel_starting = false; status_.detail.clear(); emit statusChanged(); });
	connect(&tunnel, &Tunnel::failed, this, [this](const QString &why) { tunnel_starting = false; status_.detail = why; emit statusChanged(); });

	// The real tools. The controller attached through the tunnel shows up as coming from 127.0.0.1.
	static const QRegularExpression linked(QStringLiteral("Port (\\d+):[^\\n]*\\n\\s*[^\\n]*?\\s*\\(054c:(?:0ce6|0df2)\\)\\s*-> usbip://127\\.0\\.0\\.1:\\d+/"), QRegularExpression::CaseInsensitiveOption);
	is_attached = [this](std::function<void(bool)> done) {
		runTool(this, findTool(QStringLiteral("usbip"), QStringLiteral("USBip")), { QStringLiteral("port") }, [done](int, const QString &out) { done(linked.match(out).hasMatch()); });
	};
	attach = [this](const QString &bus, std::function<void(bool, QString)> done) {
		// --once: fail now instead of letting the driver retry in the background; tick() does the retrying.
		runTool(this, findTool(QStringLiteral("usbip"), QStringLiteral("USBip")),
			{ QStringLiteral("--tcp-port"), QString::number(usb_port), QStringLiteral("attach"), QStringLiteral("-r"), QStringLiteral("127.0.0.1"), QStringLiteral("-b"), bus, QStringLiteral("--once") },
			[done](int code, const QString &out) { done(code == 0, code == 0 ? QString() : QStringLiteral("Attach failed: ") + out.section(QLatin1Char('\n'), -1).trimmed()); });
	};
	detach = [this] {
		const QString usbip = findTool(QStringLiteral("usbip"), QStringLiteral("USBip"));
		runTool(this, usbip, { QStringLiteral("port") }, [this, usbip](int, const QString &out) {
			const QRegularExpressionMatch m = linked.match(out);
			if(m.hasMatch())
				runTool(this, usbip, { QStringLiteral("detach"), QStringLiteral("-p"), QString::number(m.captured(1).toInt()) }, [](int, const QString &) {});
		});
	};
}

bool Host::start(QString *error)
{
	store.load();
	if(!host_server.listen(QHostAddress::LocalHost, quint16(host_port)) || !usb_server.listen(QHostAddress::LocalHost, quint16(usb_port)))
	{
		host_server.close();
		usb_server.close();
		if(error)
			*error = QStringLiteral("Another copy of Farside is already running on this PC.");
		return false;
	}
	if(direct)
		openDirect();
	ticker.start(tick_ms);
	tick();
	return true;
}

// The port other PCs on the network can reach, and the answer to their lookups.
void Host::openDirect()
{
	const bool tcp = lan_server.isListening() || lan_server.listen(lan_address, quint16(lan_port));
	const bool udp = lookup_socket.state() == QAbstractSocket::BoundState
		|| lookup_socket.bind(lan_address, quint16(lookup_port), QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
	if(!tcp)
	{
		status_.detail = QStringLiteral("Direct connections: port %1 is in use on this PC.").arg(lan_port);
		emit statusChanged();
	}
	Q_UNUSED(udp); // without it Hosts are not found by themselves; an address still works
}

void Host::setDirect(bool on)
{
	direct = on;
	if(on && host_server.isListening())
		openDirect();
	else if(!on)
	{
		lan_server.close();
		lookup_socket.close();
	}
}

void Host::lookupIncoming()
{
	while(lookup_socket.hasPendingDatagrams())
	{
		const QNetworkDatagram d = lookup_socket.receiveDatagram(64);
		if(isLookupQuestion(d.data()))
			lookup_socket.writeDatagram(lookupAnswer(store.my_id, myName(), lan_port, !pair_secret.isEmpty() && now() < pair_until), d.senderAddress(), quint16(d.senderPort()));
	}
}

void Host::stop()
{
	ticker.stop();
	lan_server.close();
	lookup_socket.close();
	host_server.close();
	usb_server.close();
	if(control)
		control->abort();
	const QList<QTcpSocket *> waiting = pending.values();
	pending.clear();
	for(QTcpSocket *s : waiting)
		s->abort();
	tunnel.stop();
	tunnel_starting = false;
	ticket.clear();
}

const Peer *Host::findPeer(const QString &id) const
{
	for(const Peer &p : store.peers)
		if(p.id == id)
			return &p;
	return nullptr;
}

QString Host::beginPairing()
{
	const QString t = use_tunnel ? ticket : QStringLiteral("selftestticket0000000");
	if(t.isEmpty())
		return QString();
	pair_secret = randomBytes(16);
	pair_until = now().addSecs(600);
	return makeCode(t, pair_secret);
}

int Host::pairingSecondsLeft() const
{
	return pair_secret.isEmpty() ? 0 : int(qMax<qint64>(0, now().secsTo(pair_until)));
}

void Host::incoming()
{
	for(QTcpServer *server : {&host_server, &lan_server})
	while(QTcpSocket *sock = server->nextPendingConnection())
	{
		// From the network (not through the tunnel, which ends on this PC): nothing secret may cross it.
		sock->setProperty("farside_lan", server == &lan_server);
		sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);
		const QString nonce = hex(randomBytes(16));
		connect(sock, &QTcpSocket::readyRead, this, [this, sock, nonce] {
			if(sock->property("farside_phase").toInt() == 0)
				handshake(sock, nonce);
			QString line;
			while(sock == control && takeLine(sock, &line))
				controlLine(line);
		});
		connect(sock, &QTcpSocket::disconnected, this, [this, sock] {
			if(sock == asking)
			{
				asking = nullptr;
				ask_timeout.stop();
			}
			dropControl(sock);
			sock->deleteLater();
		});
		// Whoever connects has 15 seconds to say who they are.
		QTimer::singleShot(15000, sock, [sock] { if(sock->property("farside_phase").toInt() == 0) sock->abort(); });
		send(sock, QStringLiteral("NONCE ") + nonce);
	}
}

void Host::handshake(QTcpSocket *sock, const QString &nonce)
{
	QString line;
	if(!takeLine(sock, &line))
		return;
	const QStringList p = splitLine(line);
	if(p[0] == QLatin1String("PAIR") && p.size() == 5)
	{
		// This line carries the code's secret and the new key: only inside the encrypted tunnel.
		if(sock->property("farside_lan").toBool())
		{
			send(sock, QStringLiteral("NO rejected"));
			sock->disconnectFromHost();
			return;
		}
		pairLine(sock, p);
		return;
	}
	if(p[0] == QLatin1String("PAIRX") && p.size() == 4)
	{
		pairDirectLine(sock, p, nonce);
		return;
	}
	const Peer *peer = p.size() >= 3 ? findPeer(p[1]) : nullptr;
	const bool proven = peer && sameText(proof(peer->key, nonce), p[2]);
	if(p[0] == QLatin1String("AUTH") && p.size() == 3)
	{
		if(!proven)
		{
			send(sock, QStringLiteral("NO unknown"));
			sock->disconnectFromHost();
			return;
		}
		// The same Client coming back (its old connection may not have timed out yet) replaces itself.
		if(control && status_.peer_id != peer->id)
		{
			send(sock, QStringLiteral("NO busy"));
			sock->disconnectFromHost();
			return;
		}
		QTcpSocket *old = control;
		control = sock;
		if(old)
			old->abort();
		sock->setProperty("farside_phase", 1);
		status_.peer_id = peer->id;
		status_.peer_name = peer->name;
		status_.bus.clear();
		status_.controller.clear();
		status_.rtt_ms = -1;
		send(sock, QStringLiteral("OK ") + myName());
		pinger.start();
		silence.start();
		tick();
	}
	else if(p[0] == QLatin1String("DATA") && p.size() == 4 && proven && control && status_.peer_id == peer->id && pending.contains(p[3]))
	{
		sock->setProperty("farside_phase", 3);
		bridge(pending.take(p[3]), sock);
	}
	else
		sock->abort();
}

// PAIR <secret> <clientId> <clientName> <newKey>
void Host::pairLine(QTcpSocket *sock, const QStringList &p)
{
	const bool ok = !pair_secret.isEmpty() && now() < pair_until && sameText(hex(pair_secret), p[1].toLower());
	if(ok)
		pair_secret.clear(); // one use
	if(!ok || !id_shape.match(p[2]).hasMatch() || !key_shape.match(p[4]).hasMatch())
	{
		send(sock, QStringLiteral("NO expired"));
		sock->disconnectFromHost();
		return;
	}
	if(asking || receivers(SIGNAL(pairRequested(QString,QString))) == 0)
	{
		send(sock, QStringLiteral("NO rejected"));
		sock->disconnectFromHost();
		return;
	}
	sock->setProperty("farside_phase", 2);
	asking = sock;
	asking_direct = false;
	asking_peer = { p[2], cleanName(p[3]), QByteArray::fromHex(p[4].toLatin1()) };
	ask_timeout.start();
	emit pairRequested(asking_peer.name, QString());
}

// PAIRX <clientId> <clientName> <clientPublicValue>: pairing without the tunnel. Allowed only while this
// PC's Pair page is open (the same 10 minutes a code is valid), so nobody on the network can make
// the question pop up at other times. The key is agreed on, never sent.
void Host::pairDirectLine(QTcpSocket *sock, const QStringList &p, const QString &nonce)
{
	const bool open = !pair_secret.isEmpty() && now() < pair_until;
	QByteArray my_secret, my_public, shared;
	if(open && id_shape.match(p[1]).hasMatch() && key_shape.match(p[3]).hasMatch() && agreeKeys(&my_secret, &my_public))
		shared = agreeShared(my_secret, QByteArray::fromHex(p[3].toLatin1()));
	if(shared.isEmpty())
	{
		send(sock, QStringLiteral("NO expired"));
		sock->disconnectFromHost();
		return;
	}
	if(asking || receivers(SIGNAL(pairRequested(QString,QString))) == 0)
	{
		send(sock, QStringLiteral("NO rejected"));
		sock->disconnectFromHost();
		return;
	}
	sock->setProperty("farside_phase", 2);
	asking = sock;
	asking_direct = true;
	asking_peer = { p[1], cleanName(p[2]), directKey(shared, nonce) };
	send(sock, QStringLiteral("KEY ") + hex(my_public));
	ask_timeout.start();
	emit pairRequested(asking_peer.name, checkNumber(shared, nonce));
}

void Host::answerPair(bool accept)
{
	if(!asking)
		return;
	QTcpSocket *sock = asking;
	asking = nullptr;
	ask_timeout.stop();
	if(accept)
	{
		for(int i = store.peers.size() - 1; i >= 0; i--)
			if(store.peers[i].id == asking_peer.id)
				store.peers.removeAt(i);
		store.peers.append(asking_peer);
		store.save();
		if(asking_direct)
			pair_secret.clear(); // the Pair page's 10 minutes are used up, as with a code
		send(sock, QStringLiteral("OK ") + myName());
	}
	else
		send(sock, QStringLiteral("NO rejected"));
	sock->disconnectFromHost();
	emit statusChanged();
}

void Host::unpair(const QString &id)
{
	for(int i = store.peers.size() - 1; i >= 0; i--)
		if(store.peers[i].id == id)
			store.peers.removeAt(i);
	store.save();
	if(control && status_.peer_id == id)
		control->abort();
	tick();
}

void Host::controlLine(const QString &line)
{
	silence.start();
	const QStringList q = splitLine(line);
	// The bus ID ends up on a command line, so it must look like one.
	if(q[0] == QLatin1String("BUS") && q.size() == 3 && bus_shape.match(q[1]).hasMatch())
	{
		status_.bus = q[1];
		status_.controller = QString(q[2]).replace(QLatin1Char('_'), QLatin1Char(' '));
		tick();
	}
	else if(q[0] == QLatin1String("NOBUS"))
	{
		status_.bus.clear();
		status_.controller.clear();
		tick();
	}
	else if(q[0] == QLatin1String("PONG") && q.size() == 2)
	{
		const qint64 took = QDateTime::currentMSecsSinceEpoch() % 2000000000 - q[1].toLongLong();
		if(took >= 0 && took < 60000)
		{
			status_.rtt_ms = int(took);
			emit statusChanged();
		}
	}
}

void Host::dropControl(QTcpSocket *sock)
{
	if(sock != control)
		return;
	control = nullptr;
	pinger.stop();
	silence.stop();
	status_.peer_id.clear();
	status_.peer_name.clear();
	status_.bus.clear();
	status_.controller.clear();
	status_.rtt_ms = -1;
	tick();
}

// The local usbip driver connected: ask the Client for a data connection and park this one until it arrives.
void Host::usbIncoming()
{
	while(QTcpSocket *usb = usb_server.nextPendingConnection())
	{
		usb->setSocketOption(QAbstractSocket::LowDelayOption, 1);
		if(!control)
		{
			usb->abort();
			usb->deleteLater();
			continue;
		}
		const QString token = hex(randomBytes(8));
		pending[token] = usb;
		connect(usb, &QTcpSocket::disconnected, this, [this, usb, token] { if(pending.value(token) == usb) pending.remove(token); usb->deleteLater(); });
		QTimer::singleShot(10000, usb, [this, usb, token] { if(pending.value(token) == usb) usb->abort(); });
		send(control, QStringLiteral("OPEN ") + token);
	}
}

void Host::setState(const QString &state)
{
	status_.state = state;
	emit statusChanged();
}

// Keeps the tunnel up, and attaches or detaches the controller to match what the Client reports.
void Host::tick()
{
	if(use_tunnel && !tunnel.alive() && !tunnel_starting && host_server.isListening())
	{
		tunnel_starting = true;
		ticket.clear();
		tunnel.listen(host_port, store.iroh_secret);
	}
	if(tool_busy)
		return;
	tool_busy = true;
	is_attached([this](bool attached) {
		tool_busy = false;
		const bool up = control != nullptr;
		if(attached && (!up || status_.bus.isEmpty()))
		{
			detach();
			attached = false;
		}
		else if(!attached && up && !status_.bus.isEmpty() && keep_linked)
		{
			tool_busy = true;
			attach(status_.bus, [this](bool ok, const QString &error) {
				tool_busy = false;
				status_.detail = error;
				if(ok)
					tick();
				else
					emit statusChanged();
			});
		}
		setState(attached ? QStringLiteral("Linked") : up ? QStringLiteral("Client online")
			: !store.peers.isEmpty() ? QStringLiteral("Waiting for client") : QStringLiteral("Not paired"));
	});
}

// ---- Client ----

Client::Client(QObject *parent) : QObject(parent), tunnel(this)
{
	connect(&ticker, &QTimer::timeout, this, &Client::tick);
	retry.setSingleShot(true);
	connect(&retry, &QTimer::timeout, this, &Client::connectControl);
	silence.setSingleShot(true);
	silence.setInterval(15000);
	connect(&silence, &QTimer::timeout, this, [this] { if(control) control->abort(); });
	connect(&tunnel, &Tunnel::failed, this, [this](const QString &why) { status_.detail = why; emit statusChanged(); });
	connect(&lookup_socket, &QUdpSocket::readyRead, this, [this] {
		while(lookup_socket.hasPendingDatagrams())
		{
			const QNetworkDatagram d = lookup_socket.receiveDatagram(256);
			QString id, name;
			int port = 0;
			bool open = false;
			if(parseLookupAnswer(d.data(), &id, &name, &port, &open))
				emit hostFound(id, name, QHostAddress(d.senderAddress().toIPv4Address()).toString(), port, open);
		}
	});

	// The real tool: { bus ID, controller name, state } of a shared DualSense (054c:0ce6) or DualSense Edge (054c:0df2).
	read_bus = [this](std::function<void(QStringList)> done) {
#ifdef Q_OS_WIN
		static const QRegularExpression row(QStringLiteral("^\\s*(\\d+-[\\d.]+)\\s+(054c:(?:0ce6|0df2))\\s+(\\S[^\\r\\n]*?)\\s{2,}(\\S[^\\r\\n]*?)\\s*$"),
			QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
		runTool(this, findTool(QStringLiteral("usbipd"), QStringLiteral("usbipd-win")), { QStringLiteral("list") }, [done](int, const QString &out) {
			const QRegularExpressionMatch m = row.match(out);
			const QString state = m.captured(4);
			if(m.hasMatch() && (state.startsWith(QLatin1String("Shared")) || state.startsWith(QLatin1String("Attached"))))
				done({ m.captured(1), m.captured(3), state });
			else
				done({});
		});
#else
		// Linux: the kernel shows it. A device bound to usbip-host is shared; usbip_status 2 means the other PC has it.
		const QDir devices(QStringLiteral("/sys/bus/usb/devices"));
		auto read = [](const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? QString::fromLatin1(f.readAll()).trimmed() : QString(); };
		for(const QString &name : devices.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
		{
			const QString dir = devices.filePath(name);
			const QString product = read(dir + QStringLiteral("/idProduct"));
			if(read(dir + QStringLiteral("/idVendor")) != QLatin1String("054c") || (product != QLatin1String("0ce6") && product != QLatin1String("0df2")))
				continue;
			if(QFileInfo(dir + QStringLiteral("/driver")).symLinkTarget().section(QLatin1Char('/'), -1) != QLatin1String("usbip-host"))
				break;
			done({ name, QStringLiteral("DualSense"), read(dir + QStringLiteral("/usbip_status")) == QLatin1String("2") ? QStringLiteral("Attached") : QStringLiteral("Shared") });
			return;
		}
		done({});
#endif
	};
}

void Client::setState(const QString &state)
{
	status_.state = state;
	emit statusChanged();
}

void Client::start()
{
	store.load();
	running = true;
	wait_ms = backoff_ms;
	silence.setInterval(silence_ms);
	connectControl();
}

void Client::stop()
{
	running = false;
	ticker.stop();
	retry.stop();
	silence.stop();
	if(control)
		control->abort();
	tunnel.stop();
}

// Where a connection to the Host goes: straight to its address, or into the tunnel's end on this PC.
void Client::dial(QTcpSocket *s, const QString &ticket)
{
	QString host;
	int port = 0;
	if(parseDirect(ticket, &host, &port))
		s->connectToHost(host, quint16(port));
	else
		s->connectToHost(QHostAddress::LocalHost, quint16(target()));
}

void Client::ensureTunnel(const QString &ticket, std::function<void()> then)
{
	if(parseDirect(ticket, nullptr, nullptr))
	{
		tunnel.stop();
		then();
		return;
	}
	if(!use_tunnel || (tunnel.alive() && tunnel.target() == ticket))
	{
		then();
		return;
	}
	tunnel.connectTo(client_port, ticket, store.iroh_secret);
	// dumbpipe needs a moment before it listens on the local port.
	QTimer::singleShot(700, this, then);
}

void Client::connectControl()
{
	if(!running || pairing)
		return;
	if(store.host_ticket.isEmpty())
	{
		setState(QStringLiteral("Not paired"));
		return;
	}
	if(status_.state != QLatin1String("Host busy"))
		setState(QStringLiteral("Connecting"));
	ensureTunnel(store.host_ticket, [this] {
		if(!running || pairing || control)
			return;
		accepted = false;
		auto *s = new QTcpSocket(this);
		control = s;
		s->setSocketOption(QAbstractSocket::LowDelayOption, 1);
		connect(s, &QTcpSocket::readyRead, this, [this, s] {
			QString line;
			while(control == s && takeLine(s, &line))
				controlLine(line);
		});
		auto closed = [this, s] {
			if(control != s)
				return;
			control = nullptr;
			ticker.stop();
			silence.stop();
			s->deleteLater();
			retryLater(accepted);
		};
		// Every ending goes through this state, also abort() on a connection that was still being made
		// (a Host that is away), which sends neither disconnected() nor errorOccurred().
		connect(s, &QTcpSocket::stateChanged, this, [closed](QAbstractSocket::SocketState state) { if(state == QAbstractSocket::UnconnectedState) closed(); });
		silence.start();
		dial(s, store.host_ticket);
	});
}

void Client::controlLine(const QString &line)
{
	silence.start();
	if(!accepted)
	{
		if(line.startsWith(QLatin1String("NONCE ")))
			send(control, QStringLiteral("AUTH %1 %2").arg(store.my_id, proof(store.host_key, line.mid(6))));
		else if(line.startsWith(QLatin1String("OK")))
		{
			accepted = true;
			wait_ms = backoff_ms;
			status_.detail.clear();
			status_.peer_name = store.host_name;
			last_bus_line.clear();
			setState(QStringLiteral("Host online"));
			ticker.start(tick_ms);
			tick();
		}
		else
		{
			if(line == QLatin1String("NO busy"))
				setState(QStringLiteral("Host busy"));
			else if(line == QLatin1String("NO unknown"))
				status_.detail = QStringLiteral("The Host no longer knows this PC. Pair again.");
			control->abort();
		}
		return;
	}
	static const QRegularExpression token_shape(QStringLiteral("^[0-9a-f]{16}$"));
	const QStringList q = splitLine(line);
	if(q[0] == QLatin1String("OPEN") && q.size() == 2 && token_shape.match(q[1]).hasMatch())
		openData(q[1]);
	else if(q[0] == QLatin1String("PING") && q.size() == 2)
		send(control, QStringLiteral("PONG ") + q[1]);
}

// Retry after 3 s, then 6, 12, up to 30 s; a connection that worked resets it.
void Client::retryLater(bool worked)
{
	if(!running || pairing)
		return;
	if(status_.state == QLatin1String("Host online") || status_.state == QLatin1String("Linked"))
		setState(QStringLiteral("Connecting"));
	else
		emit statusChanged();
	const int delay = worked ? backoff_ms : wait_ms;
	wait_ms = worked ? backoff_ms : qMin(wait_ms * 2, 30000);
	retry.start(delay);
}

// Reports the controller whenever it changes, and keeps the connection alive.
void Client::tick()
{
	if(!control || !accepted)
		return;
	read_bus([this](const QStringList &b) {
		if(!control || !accepted)
			return;
		const bool shared = b.size() == 3;
		const QString msg = shared ? QStringLiteral("BUS %1 %2").arg(b[0], cleanName(b[1])) : QStringLiteral("NOBUS");
		if(msg != last_bus_line)
		{
			send(control, msg);
			last_bus_line = msg;
		}
		status_.bus = shared ? b[0] : QString();
		status_.controller = shared ? b[1] : QString();
		setState(shared && b[2].startsWith(QLatin1String("Attached")) ? QStringLiteral("Linked") : QStringLiteral("Host online"));
		send(control, QStringLiteral("PING"));
	});
}

// The Host asked for a data connection: open it and join it to the local usbipd.
void Client::openData(const QString &token)
{
	auto *d = new QTcpSocket(this);
	connect(d, &QTcpSocket::disconnected, d, &QObject::deleteLater);
	connect(d, &QTcpSocket::readyRead, this, [this, d, token] {
		QString line;
		if(d->property("farside_sent").toBool() || !takeLine(d, &line))
			return;
		if(!line.startsWith(QLatin1String("NONCE ")))
		{
			d->abort();
			return;
		}
		d->setProperty("farside_sent", true);
		auto *usbipd = new QTcpSocket(this);
		connect(usbipd, &QTcpSocket::connected, this, [this, d, usbipd, line, token] {
			send(d, QStringLiteral("DATA %1 %2 %3").arg(store.my_id, proof(store.host_key, line.mid(6)), token));
			bridge(d, usbipd);
		});
		connect(usbipd, &QTcpSocket::errorOccurred, this, [d, usbipd](QAbstractSocket::SocketError) {
			if(usbipd->state() != QAbstractSocket::UnconnectedState)
				return;
			d->abort();
			usbipd->deleteLater();
		});
		usbipd->connectToHost(QHostAddress::LocalHost, quint16(usbipd_port));
	});
	dial(d, store.host_ticket);
}

void Client::pair(const QString &code)
{
	QString ticket;
	QByteArray secret;
	if(!parseCode(code, &ticket, &secret))
	{
		emit pairFinished(QStringLiteral("That is not a Farside code."));
		return;
	}
	if(pairing)
		return;
	pairing = true;
	retry.stop();
	if(control)
		control->abort();
	pair_ticket = ticket;
	pair_secret = secret;
	pair_direct = false;
	ensureTunnel(ticket, [this] { tryPair(0); });
}

void Client::pairDirect(const QString &address)
{
	QString host = address.trimmed();
	int port = lan_port;
	const int colon = host.lastIndexOf(QLatin1Char(':'));
	if(colon > 0 && host.count(QLatin1Char(':')) == 1)
	{
		port = host.mid(colon + 1).toInt();
		host = host.left(colon);
	}
	static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9._-]{1,253}$"));
	if(!plain.match(host).hasMatch() || port <= 0 || port > 65535)
	{
		emit pairFinished(QStringLiteral("That is not an address. Use a name or an IP address like 192.168.1.20."));
		return;
	}
	if(pairing)
		return;
	pairing = true;
	retry.stop();
	if(control)
		control->abort();
	pair_ticket = makeDirect(host, port);
	pair_direct = true;
	pair_has_key = false;
	ensureTunnel(pair_ticket, [this] { tryPair(0); });
}

void Client::lookup()
{
	if(lookup_socket.state() != QAbstractSocket::BoundState && !lookup_socket.bind(lookup_bind, 0))
		return;
	const QByteArray question = lookupQuestion();
	// to everyone on every network this PC is on, and to this PC itself (a Host on the same PC)
	lookup_socket.writeDatagram(question, QHostAddress::LocalHost, quint16(lookup_port));
	if(lookup_bind == QHostAddress::LocalHost)
		return;
	lookup_socket.writeDatagram(question, QHostAddress::Broadcast, quint16(lookup_port));
	for(const QNetworkInterface &net : QNetworkInterface::allInterfaces())
		if((net.flags() & QNetworkInterface::IsUp) && (net.flags() & QNetworkInterface::CanBroadcast))
			for(const QNetworkAddressEntry &entry : net.addressEntries())
				if(!entry.broadcast().isNull())
					lookup_socket.writeDatagram(question, entry.broadcast(), quint16(lookup_port));
}

// The first connections can fail while iroh is still finding the Host, so this tries a few times.
void Client::tryPair(int attempt)
{
	if(!pairing)
		return;
	// The tunnel needs time to find the Host; an address answers at once or not at all.
	if(attempt >= (pair_direct ? 3 : 8))
	{
		finishPair(pair_direct ? QStringLiteral("Nothing answered at that address. On the Host, direct connections must be on (Remote Play, Pair) and Windows must allow Farside through its firewall.")
			: QStringLiteral("Could not reach the Host."));
		return;
	}
	auto *s = new QTcpSocket(this);
	pair_sock = s;
	auto ended = [this, s, attempt] {
		if(pair_sock != s)
			return;
		pair_sock = nullptr;
		const int stage = s->property("farside_stage").toInt();
		s->deleteLater();
		if(stage == 0)
			QTimer::singleShot(1000, this, [this, attempt] { tryPair(attempt + 1); });
		else if(stage == 1)
			finishPair(QStringLiteral("Could not reach the Host."));
	};
	connect(s, &QTcpSocket::disconnected, this, ended);
	connect(s, &QTcpSocket::errorOccurred, this, [s, ended](QAbstractSocket::SocketError) { if(s->state() == QAbstractSocket::UnconnectedState) ended(); });
	connect(s, &QTcpSocket::readyRead, this, [this, s] {
		QString line;
		if(!takeLine(s, &line))
			return;
		if(s->property("farside_stage").toInt() == 0)
		{
			if(!line.startsWith(QLatin1String("NONCE ")))
			{
				s->abort();
				return;
			}
			if(pair_direct)
			{
				// No tunnel around this connection: the key is agreed on, not sent.
				QByteArray my_public;
				if(!agreeKeys(&pair_secret_key, &my_public))
				{
					s->abort();
					return;
				}
				pair_nonce = line.mid(6);
				s->setProperty("farside_stage", 1);
				send(s, QStringLiteral("PAIRX %1 %2 %3").arg(store.my_id, myName(), hex(my_public)));
				QTimer::singleShot(75000, s, [s] { s->abort(); });
				return;
			}
			// The tunnel is encrypted to the Host's key, so the new pairing key can travel inside it.
			pair_key = randomBytes(32);
			s->setProperty("farside_stage", 1);
			send(s, QStringLiteral("PAIR %1 %2 %3 %4").arg(hex(pair_secret), store.my_id, myName(), hex(pair_key)));
			// The Host's user has 60 seconds to click Accept.
			QTimer::singleShot(75000, s, [s] { s->abort(); });
			return;
		}
		if(pair_direct && !pair_has_key && line.startsWith(QLatin1String("KEY ")))
		{
			const QByteArray shared = agreeShared(pair_secret_key, QByteArray::fromHex(line.mid(4).trimmed().toLatin1()));
			if(shared.isEmpty())
			{
				s->abort();
				return;
			}
			pair_key = directKey(shared, pair_nonce);
			pair_has_key = true;
			emit pairCheck(checkNumber(shared, pair_nonce));
			return; // now the Host's user compares the number and answers
		}
		s->setProperty("farside_stage", 2);
		if(line.startsWith(QLatin1String("OK")) && (!pair_direct || pair_has_key))
		{
			store.host_ticket = pair_ticket;
			store.host_key = pair_key;
			store.host_name = line.size() > 3 ? cleanName(line.mid(3)) : QStringLiteral("Host");
			store.save();
			finishPair(QString());
		}
		else if(line == QLatin1String("NO rejected"))
			finishPair(QStringLiteral("The Host rejected this PC."));
		else if(line == QLatin1String("NO expired"))
			finishPair(pair_direct ? QStringLiteral("The Host is not ready to pair. Open Remote Play, Pair on the Host first.")
				: QStringLiteral("This code has expired. Make a new one on the Host."));
		else
			finishPair(QStringLiteral("Could not reach the Host."));
		s->disconnectFromHost();
	});
	// An address on a network or VPN answers within a second; the tunnel can take several.
	// abort() says nothing when the connection was still being made, so the ending is called by hand
	// (it does nothing the second time).
	QTimer::singleShot(pair_direct ? pair_connect_ms : 10000, s, [s, ended] {
		if(s->property("farside_stage").toInt() != 0)
			return;
		s->abort();
		ended();
	});
	dial(s, pair_ticket);
}

void Client::finishPair(const QString &problem)
{
	if(!pairing)
		return;
	pairing = false;
	wait_ms = backoff_ms;
	emit pairFinished(problem);
	if(running)
		connectControl();
}

void Client::unpair()
{
	store.host_ticket.clear();
	store.host_name.clear();
	store.host_key.clear();
	store.save();
	retry.stop();
	if(control)
		control->abort();
	status_ = Status();
	emit statusChanged();
}

// ---- self-test, checks 15 to 23: Host and Client in this process over real loopback sockets ----
// The sessions run on the main thread's event loop. The checks run on a second thread with blocking
// sockets of their own, and reach into the sessions only through onMain().

static QString readLineBlocking(QTcpSocket &s, int timeout_ms)
{
	while(!s.canReadLine())
		if(!s.waitForReadyRead(timeout_ms))
			return QString();
	return QString::fromUtf8(s.readLine(600)).trimmed();
}

static QByteArray readExact(QTcpSocket &s, int count)
{
	while(s.bytesAvailable() < count)
		if(!s.waitForReadyRead(5000))
			return QByteArray();
	return s.read(count);
}

int selfTestSessions()
{
	QTemporaryDir dir;
	Host host;
	Client client;
	host.store.path = dir.filePath(QStringLiteral("host.dat"));
	client.store.path = dir.filePath(QStringLiteral("client.dat"));
	std::atomic<qint64> skew_ms(0);
	host.use_tunnel = false;
	host.host_port = 47650;
	host.usb_port = 47651;
	host.tick_ms = 100;
	host.now = [&skew_ms] { return QDateTime::currentDateTimeUtc().addMSecs(skew_ms); };
	host.attach = [](const QString &, std::function<void(bool, QString)> done) { done(false, QString()); };
	host.is_attached = [](std::function<void(bool)> done) { done(false); };
	host.detach = [] {};
	client.use_tunnel = false;
	client.direct_port = 47650;
	client.usbipd_port = 47653;
	client.tick_ms = 100;
	client.backoff_ms = 200;
	client.read_bus = [](std::function<void(QStringList)> done) { done({ QStringLiteral("1-1"), QStringLiteral("Test Pad"), QStringLiteral("Shared") }); };
	QObject::connect(&host, &Host::pairRequested, &host, [&host] { host.answerPair(true); });
	std::atomic<int> pair_done(0);
	QString pair_problem;
	QObject::connect(&client, &Client::pairFinished, &client, [&](const QString &problem) { pair_problem = problem; pair_done = 1; });
	if(!host.start())
		return 15;

	auto onMain = [&host](std::function<void()> f) { QMetaObject::invokeMethod(&host, f, Qt::BlockingQueuedConnection); };
	auto waitFor = [](std::function<bool()> done, int timeout_ms) {
		for(int waited = 0; waited < timeout_ms; waited += 50)
		{
			if(done())
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		return done();
	};
	auto hostState = [&] { QString s; onMain([&] { s = host.status().state; }); return s; };
	auto clientState = [&] { QString s; onMain([&] { s = client.status().state; }); return s; };

	int result = 0;
	std::thread checks([&] {
		result = [&]() -> int {
			const QString k1(64, QLatin1Char('a')), k2(64, QLatin1Char('b'));
			QString code, ticket, nonce;
			QByteArray secret;
			// Opens a raw connection and reads the Host's nonce.
			auto raw = [&nonce](QTcpSocket &s) {
				s.connectToHost(QHostAddress::LocalHost, 47650);
				if(!s.waitForConnected(3000))
					return false;
				const QString l = readLineBlocking(s, 5000);
				nonce = l.startsWith(QLatin1String("NONCE ")) ? l.mid(6) : QString();
				return !nonce.isEmpty();
			};
			auto say = [](QTcpSocket &s, const QString &line) { s.write((line + QLatin1Char('\n')).toUtf8()); s.flush(); };
			auto hasPeer = [&](const QString &id) { bool found = false; onMain([&] { for(const Peer &p : host.store.peers) found = found || p.id == id; }); return found; };

			onMain([&] { code = host.beginPairing(); });
			if(!parseCode(code, &ticket, &secret)) return 15;
			const QString pair = QStringLiteral("PAIR %1 aaaaaaaaaaaaaaaa RawPC %2").arg(hex(secret), k1);
			{
				QTcpSocket a;
				if(!raw(a)) return 15;
				say(a, pair);
				if(!readLineBlocking(a, 5000).startsWith(QLatin1String("OK ")) || !hasPeer(QStringLiteral("aaaaaaaaaaaaaaaa"))) return 15;
			}
			{
				QTcpSocket a;
				if(!raw(a)) return 16;
				say(a, pair); // the same code a second time
				if(readLineBlocking(a, 5000) != QLatin1String("NO expired")) return 16;
			}
			onMain([&] { code = host.beginPairing(); });
			parseCode(code, &ticket, &secret);
			skew_ms = 11 * 60 * 1000;
			{
				QTcpSocket a;
				if(!raw(a)) return 17;
				say(a, QStringLiteral("PAIR %1 cccccccccccccccc LatePC %2").arg(hex(secret), k1));
				if(readLineBlocking(a, 5000) != QLatin1String("NO expired") || hasPeer(QStringLiteral("cccccccccccccccc"))) return 17;
			}
			skew_ms = 0;
			{
				QTcpSocket a;
				if(!raw(a)) return 18;
				say(a, QStringLiteral("AUTH aaaaaaaaaaaaaaaa ") + proof(QByteArray::fromHex(k2.toLatin1()), nonce)); // wrong key
				if(readLineBlocking(a, 5000) != QLatin1String("NO unknown") || !readLineBlocking(a, 2000).isEmpty()) return 18;
			}
			{
				QTcpSocket a, b;
				if(!raw(a)) return 19;
				say(a, QStringLiteral("AUTH aaaaaaaaaaaaaaaa ") + proof(QByteArray::fromHex(k1.toLatin1()), nonce));
				if(!readLineBlocking(a, 5000).startsWith(QLatin1String("OK"))) return 19;
				onMain([&] { host.store.peers.append({ QStringLiteral("bbbbbbbbbbbbbbbb"), QStringLiteral("Second"), QByteArray::fromHex(k2.toLatin1()) }); });
				if(!raw(b)) return 19;
				say(b, QStringLiteral("AUTH bbbbbbbbbbbbbbbb ") + proof(QByteArray::fromHex(k2.toLatin1()), nonce));
				// The second Client is turned away, and the first one still gets the Host's PING.
				if(readLineBlocking(b, 5000) != QLatin1String("NO busy")) return 19;
				say(a, QStringLiteral("PING"));
				if(!readLineBlocking(a, 7000).startsWith(QLatin1String("PING"))) return 19;
			}
			if(!waitFor([&] { return hostState() != QLatin1String("Client online"); }, 3000)) return 19;

			onMain([&] { code = host.beginPairing(); client.store.load(); client.pair(code); });
			QString host_ticket;
			if(!waitFor([&] { return pair_done == 1; }, 15000)) return 20;
			onMain([&] { host_ticket = client.store.host_ticket; });
			if(!pair_problem.isEmpty() || host_ticket.isEmpty()) return 20;
			pair_done = 0;
			onMain([&] { client.pair(QStringLiteral("nonsense")); });
			QString after;
			onMain([&] { after = client.store.host_ticket; });
			if(pair_done != 1 || pair_problem != QLatin1String("That is not a Farside code.") || after != host_ticket) return 21;

			QTcpServer fake_usbipd;
			if(!fake_usbipd.listen(QHostAddress::LocalHost, 47653)) return 22;
			onMain([&] { client.start(); });
			QString bus;
			if(!waitFor([&] { onMain([&] { bus = host.status().bus; }); return clientState() == QLatin1String("Host online") && bus == QLatin1String("1-1"); }, 8000)) return 22;
			{
				QTcpSocket usbip;
				usbip.connectToHost(QHostAddress::LocalHost, 47651);
				if(!usbip.waitForConnected(3000) || !fake_usbipd.waitForNewConnection(8000)) return 22;
				std::unique_ptr<QTcpSocket> served(fake_usbipd.nextPendingConnection());
				const QByteArray there = randomBytes(1000), home = randomBytes(1000);
				usbip.write(there);
				usbip.flush();
				served->write(home);
				served->flush();
				if(readExact(*served, 1000) != there || readExact(usbip, 1000) != home) return 22;
			}

			onMain([&] { host.stop(); });
			if(!waitFor([&] { return clientState() != QLatin1String("Host online"); }, 5000)) return 23;
			bool started = false;
			onMain([&] { started = host.start(); }); // nothing tells the Client; it has to come back by itself
			if(!started || !waitFor([&] { return clientState() == QLatin1String("Host online"); }, 10000)) return 23;
			return 0;
		}();
		QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
	});
	QCoreApplication::exec();
	checks.join();
	client.stop();
	host.stop();
	return result;
}

// ---- self-test, checks 30 to 37: pairing and linking without the tunnel ----

static bool waitFor(const std::function<bool()> &done, int ms)
{
	QElapsedTimer t;
	t.start();
	while(!done() && t.elapsed() < ms)
		QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
	return done();
}

int selfTestDirect()
{
	// 30: two PCs agree on a key without sending it.
	QByteArray a_secret, a_public, b_secret, b_public;
	if(!agreeKeys(&a_secret, &a_public) || !agreeKeys(&b_secret, &b_public))
		return 30;
	const QByteArray ab = agreeShared(a_secret, b_public), ba = agreeShared(b_secret, a_public);
	const QString n1 = QStringLiteral("00ff"), n2 = QStringLiteral("00fe");
	if(ab.size() != 32 || ab != ba || a_public == b_public || checkNumber(ab, n1).size() != 4 || checkNumber(ab, n1) != checkNumber(ba, n1)
		|| directKey(ab, n1).size() != 32 || directKey(ab, n1) == directKey(ab, n2) || !agreeShared(a_secret, QByteArray(5, 'x')).isEmpty())
		return 30;

	// 31: addresses and lookup texts.
	QString text, id, name;
	int port = 0;
	bool open = false;
	if(!parseDirect(makeDirect(QStringLiteral("192.168.1.20"), 47610), &text, &port) || text != QLatin1String("192.168.1.20") || port != 47610
		|| parseDirect(QStringLiteral("abc234ticket"), &text, &port) || parseDirect(QStringLiteral("direct:host:0"), &text, &port))
		return 31;
	if(!isLookupQuestion(lookupQuestion()) || isLookupQuestion("hello")
		|| !parseLookupAnswer(lookupAnswer(QStringLiteral("0123456789abcdef"), QStringLiteral("Home PC"), 47610, true), &id, &name, &port, &open)
		|| id != QLatin1String("0123456789abcdef") || name != QLatin1String("Home_PC") || port != 47610 || !open
		|| parseLookupAnswer("FARSIDE! 1 short Home 47610 1", &id, &name, &port, &open))
		return 31;

	QTemporaryDir dir;
	Host host;
	host.use_tunnel = false;
	host.host_port = 47660;
	host.usb_port = 47661;
	host.lan_port = 47662;
	host.lookup_port = 47663;
	host.lan_address = QHostAddress::LocalHost; // this PC only: a test must not make Windows ask about the firewall
	host.direct = true;
	host.keep_linked = false;
	host.store.path = dir.filePath(QStringLiteral("host.dat"));
	host.is_attached = [](std::function<void(bool)> done) { done(false); };
	host.attach = [](const QString &, std::function<void(bool, QString)> done) { done(false, QString()); };
	host.detach = [] {};
	QString asked, host_number;
	QObject::connect(&host, &Host::pairRequested, &host, [&](const QString &who, const QString &check) { asked = who; host_number = check; });
	if(!host.start())
		return 32;

	Client client;
	client.use_tunnel = false;
	client.lan_port = 47662;
	client.lookup_port = 47663;
	client.lookup_bind = QHostAddress::LocalHost;
	client.backoff_ms = 300;
	client.store.path = dir.filePath(QStringLiteral("client.dat"));
	client.read_bus = [](std::function<void(QStringList)> done) { done({}); };
	QString problem, client_number, found_id;
	int found_port = 0;
	bool finished = false, found_open = true;
	QObject::connect(&client, &Client::pairFinished, &client, [&](const QString &why) { problem = why; finished = true; });
	QObject::connect(&client, &Client::pairCheck, &client, [&](const QString &number) { client_number = number; });
	QObject::connect(&client, &Client::hostFound, &client, [&](const QString &host_id, const QString &, const QString &, int host_port, bool pairing_open) {
		found_id = host_id; found_port = host_port; found_open = pairing_open; });
	client.start();

	// 32: the Host's Pair page is not open: nobody on the network can make it ask.
	client.pairDirect(QStringLiteral("127.0.0.1:47662"));
	if(!waitFor([&] { return finished; }, 8000) || problem.isEmpty() || !asked.isEmpty())
		return 32;

	// 33: the lookup finds the Host and says that it is not pairing right now.
	client.lookup();
	if(!waitFor([&] { return !found_id.isEmpty(); }, 3000) || found_id != host.store.my_id || found_port != 47662 || found_open)
		return 33;

	// 34: Pair page open: both PCs show the same number before anyone accepts.
	if(host.beginPairing().isEmpty())
		return 34;
	finished = false;
	problem = QStringLiteral("?");
	client.pairDirect(QStringLiteral("127.0.0.1:47662"));
	if(!waitFor([&] { return !asked.isEmpty() && !client_number.isEmpty(); }, 5000) || host_number.size() != 4 || host_number != client_number || finished)
		return 34;

	// 35: accepted: both hold the same key, and it never crossed the connection. The Client remembers the address.
	host.answerPair(true);
	if(!waitFor([&] { return finished; }, 5000) || !problem.isEmpty())
		return 35;
	if(host.store.peers.size() != 1 || host.store.peers[0].key.size() != 32 || host.store.peers[0].key != client.store.host_key
		|| !parseDirect(client.store.host_ticket, &text, &port) || text != QLatin1String("127.0.0.1") || port != 47662)
		return 35;
	Store reread;
	reread.path = client.store.path;
	reread.load();
	if(reread.host_ticket != client.store.host_ticket || reread.host_key != client.store.host_key)
		return 35;

	// 36: the Client links straight to the Host, no tunnel.
	if(!waitFor([&] { return client.status().state == QLatin1String("Host online"); }, 6000))
	{
		fprintf(stderr, "direct link: Client state is '%s'\n", qPrintable(client.status().state));
		return 36;
	}

	// 37: the line that carries secrets (pairing by code) is refused on the network port.
	host.beginPairing();
	QTcpSocket raw;
	raw.connectToHost(QHostAddress::LocalHost, 47662);
	QByteArray got;
	QObject::connect(&raw, &QTcpSocket::readyRead, &raw, [&] { got += raw.readAll(); });
	if(!waitFor([&] { return got.contains('\n'); }, 3000))
		return 37;
	got.clear();
	raw.write("PAIR 00112233445566778899aabbccddeeff 0123456789abcdef Laptop " + QByteArray(64, 'a') + "\n");
	if(!waitFor([&] { return got.contains('\n'); }, 3000) || !got.startsWith("NO rejected") || host.store.peers.size() != 1)
		return 37;

	// 38: an address where nothing answers ends with a message. Giving up on a connection that is still
	// being made must count as an ending too (Windows takes about 2 s to refuse; a dead address never does).
	finished = false;
	problem.clear();
	client.pair_connect_ms = 300;
	client.pairDirect(QStringLiteral("127.0.0.1:47669"));
	if(!waitFor([&] { return finished; }, 8000) || problem.isEmpty())
		return 38;
	client.stop();

	// 39: a paired Host that is away (nothing at its address) is tried again and again. Giving up on a try
	// that was still connecting must not end the trying: once something listens there, a connection comes.
	Client away;
	away.use_tunnel = false;
	away.backoff_ms = 300;
	away.silence_ms = 300;
	away.store.path = dir.filePath(QStringLiteral("away.dat"));
	away.read_bus = [](std::function<void(QStringList)> done) { done({}); };
	away.store.load();
	away.store.host_ticket = makeDirect(QStringLiteral("127.0.0.1"), 47669);
	away.store.host_name = QStringLiteral("Away");
	away.store.host_key = QByteArray(32, 'k');
	away.store.save();
	away.start();
	waitFor([] { return false; }, 1500);
	QTcpServer late;
	if(!late.listen(QHostAddress::LocalHost, 47669) || !waitFor([&] { return late.hasPendingConnections(); }, 10000))
		return 39;
	away.stop();
	host.stop();
	return 0;
}

} // namespace Farside
