// Farside Host and Client sessions: the sockets, the tunnel process and the USB/IP tools.
// Ported from relay/Link.cs. Everything here is event driven and lives on one thread (the thread of
// the object), so nothing blocks: a slow tool or a slow peer never holds up the rest.
//
//   Host:   dumbpipe listen-tcp  -> forwards every incoming connection to host_port on this PC.
//   Client: dumbpipe connect-tcp -> every connection made to client_port on this PC comes out at the Host.
// Lines on those connections (one per message, UTF-8, at most 512 bytes):
//   Host -> Client   NONCE <hex>            first line on every connection
//   Client -> Host   PAIR <secret> <id> <name> <key>   |  AUTH <id> <proof>  |  DATA <id> <proof> <token>
//   Host -> Client   OK <hostName> | NO expired | NO rejected | NO unknown | NO busy
//   Client -> Host   BUS <busid> <controllerName> | NOBUS | PING | PONG <n>
//   Host -> Client   OPEN <token> | PING <n>
// After a DATA line the connection carries raw USB/IP bytes.
//
// Direct connections (no tunnel; home network or VPN): the Host also listens on lan_port for everyone.
// Everything above works there too, except PAIR, which carries secrets. Pairing there is:
//   Client -> Host   PAIRX <id> <name> <public value>
//   Host -> Client   KEY <public value>        both now show the same check number
//   Host -> Client   OK <hostName> | NO expired | NO rejected      after the Host's user answered
// Finding Hosts: the Client broadcasts a question to lookup_port (UDP), a Host answers the asker.
#pragma once

#include <farsidelink.h>

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>

#include <functional>

namespace Farside {

// What the UI shows. Read it after statusChanged().
struct Status
{
	// Host: Not paired, Waiting for client, Client online, Linked.
	// Client: Not paired, Connecting, Host online, Linked, Host busy.
	QString state = QStringLiteral("Not paired");
	QString detail; // the last problem worth showing, or empty
	QString peer_id, peer_name, bus, controller;
	int rtt_ms = -1; // Host: round trip to the connected Client
};

// A helper program: next to this program first, then in the folder it installs to, then on PATH. Empty if not found.
QString findTool(const QString &name, const QString &program_files_dir = QString());

// Runs a command without blocking and hands its exit code and output to done. A tool that cannot start gives -1.
void runTool(QObject *owner, const QString &exe, const QStringList &args, std::function<void(int, QString)> done);

// The dumbpipe child process.
class Tunnel : public QObject
{
	Q_OBJECT

	public:
		explicit Tunnel(QObject *parent = nullptr);
		~Tunnel();
		// Host: emits ready(ticket) once dumbpipe has printed this PC's address, or failed().
		void listen(int local_port, const QString &iroh_secret);
		// Client: connections to local_port now come out at the Host that owns the ticket.
		void connectTo(int local_port, const QString &ticket, const QString &iroh_secret);
		void stop();
		bool alive() const;
		QString ticket() const { return ticket_; }
		QString target() const { return target_; }

	signals:
		void ready(const QString &ticket);
		void failed(const QString &why);

	private:
		void start(const QStringList &args, const QString &iroh_secret);
		QProcess *proc = nullptr;
		QString err, ticket_, target_;
		bool listening = false;
};

class Host : public QObject
{
	Q_OBJECT

	public:
		explicit Host(QObject *parent = nullptr);

		// Replaced by the self-test: no dumbpipe, no usbip, a clock that can be moved, its own ports.
		bool use_tunnel = true;
		int host_port = 47600, usb_port = 47601, tick_ms = 3000;
		std::function<QDateTime()> now = [] { return QDateTime::currentDateTimeUtc(); };
		std::function<void(const QString &bus, std::function<void(bool ok, QString error)>)> attach;
		std::function<void(std::function<void(bool)>)> is_attached;
		std::function<void()> detach;

		Store store;
		bool keep_linked = true;
		// Direct connections: off unless the user turns them on (the first time, Windows asks whether
		// Farside may accept connections). lan_address is narrowed to this PC itself by the tests.
		bool direct = false;
		int lan_port = 47610, lookup_port = 47611;
		QHostAddress lan_address = QHostAddress::AnyIPv4;
		void setDirect(bool on);

		// False, with a sentence in error, when a local port is taken (another copy is running).
		bool start(QString *error = nullptr);
		void stop();
		// A fresh pairing code: valid 10 minutes, for one use. Empty while the tunnel is not up yet.
		QString beginPairing();
		int pairingSecondsLeft() const;
		// Answer to pairRequested(). No answer within 60 seconds is a reject.
		void answerPair(bool accept);
		void unpair(const QString &id);
		Status status() const { return status_; }

	signals:
		void statusChanged();
		// check: for a direct pairing, the number the other PC shows too; empty for a pairing by code.
		void pairRequested(const QString &name, const QString &check);

	private:
		void openDirect();
		void lookupIncoming();
		void pairDirectLine(QTcpSocket *sock, const QStringList &p, const QString &nonce);
		QTcpServer lan_server;
		QUdpSocket lookup_socket;
		bool asking_direct = false;
		void incoming();
		void handshake(QTcpSocket *sock, const QString &nonce);
		void pairLine(QTcpSocket *sock, const QStringList &p);
		void controlLine(const QString &line);
		void dropControl(QTcpSocket *sock);
		void usbIncoming();
		void tick();
		void setState(const QString &state);
		const Peer *findPeer(const QString &id) const;

		QTcpServer host_server, usb_server;
		Tunnel tunnel;
		QTimer ticker, pinger, silence;
		QString ticket;
		bool tunnel_starting = false, tool_busy = false;
		QByteArray pair_secret;
		QDateTime pair_until;
		// The one pairing request that waits for the user's answer.
		QTcpSocket *asking = nullptr;
		Peer asking_peer;
		QTimer ask_timeout;
		QTcpSocket *control = nullptr;
		// USB/IP connections from this PC's usbip driver that wait for the Client's matching DATA connection.
		QHash<QString, QTcpSocket *> pending;
		Status status_;
};

class Client : public QObject
{
	Q_OBJECT

	public:
		explicit Client(QObject *parent = nullptr);

		// Replaced by the self-test.
		bool use_tunnel = true;
		int client_port = 47602, usbipd_port = 3240, direct_port = 47600, tick_ms = 3000, backoff_ms = 3000;
		// { bus ID, controller name, state } of a shared controller on this PC, or an empty list.
		std::function<void(std::function<void(QStringList)>)> read_bus;

		Store store;

		void start();
		void stop();
		// Pairs with the Host that made the code; pairFinished() carries an empty text on success,
		// otherwise a sentence for the user.
		void pair(const QString &code);
		// Pairs with a Host that allows direct connections, by its address ("name", "1.2.3.4" or with
		// ":port"). pairCheck() gives the number to compare with the one the Host shows.
		void pairDirect(const QString &address);
		// Asks this network which Hosts are there; each answer comes as hostFound().
		void lookup();
		int lan_port = 47610, lookup_port = 47611;
		int pair_connect_ms = 4000; // how long an address may take to answer, per try
		int silence_ms = 15000;     // a link that says nothing for this long is dropped and made again
		QHostAddress lookup_bind = QHostAddress::AnyIPv4;
		void unpair();
		Status status() const { return status_; }

	signals:
		void statusChanged();
		void pairFinished(const QString &problem);
		void pairCheck(const QString &number);
		void hostFound(const QString &id, const QString &name, const QString &address, int port, bool pairing_open);

	private:
		void dial(QTcpSocket *s, const QString &ticket);
		QUdpSocket lookup_socket;
		bool pair_direct = false, pair_has_key = false;
		QByteArray pair_secret_key;
		QString pair_nonce;
		int target() const { return use_tunnel ? client_port : direct_port; }
		void ensureTunnel(const QString &ticket, std::function<void()> then);
		void connectControl();
		void controlLine(const QString &line);
		void retryLater(bool worked);
		void openData(const QString &token);
		void tick();
		void tryPair(int attempt);
		void finishPair(const QString &problem);
		void setState(const QString &state);

		Tunnel tunnel;
		QTimer ticker, retry, silence;
		QTcpSocket *control = nullptr;
		bool running = false, accepted = false, pairing = false;
		int wait_ms = 3000;
		QString last_bus_line;
		// The pairing in progress.
		QString pair_ticket;
		QByteArray pair_secret, pair_key;
		QTcpSocket *pair_sock = nullptr;
		Status status_;
};

// Checks 15 to 23 of the self-test (see farsidelink.cpp for 1 to 14). Needs a QCoreApplication.
int selfTestSessions();
// Checks 30 to 37: pairing and linking without the tunnel.
int selfTestDirect();

} // namespace Farside
