// What the Farside pages (FarsideShell.qml and friends) see of the controller link: one object, "Farside".
// The Host or Client session runs on its own thread, so a busy window never delays the controller and
// the controller never delays the window. Everything the pages read here is a copy made on that thread.
//
// For checking the pages on a PC where Farside already runs:
//   FARSIDE_SANDBOX=<folder>   key store and log live in that folder, every local port is 100 higher,
//                              a Host never asks the USB/IP driver for anything, and the pairing code
//                              on show is also written to code.txt there (for a test that plays the other PC)
//   FARSIDE_ROLE=host|client   skips the look at which USB/IP tool is installed
#pragma once

#include <farsidesession.h>
#include <farsideupdate.h>

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QQuickImageProvider>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

// image://farsideqr/<pairing code, percent-encoded>: the code as a QR picture, as wide as sourceSize asks.
class FarsideQrProvider : public QQuickImageProvider
{
	public:
		FarsideQrProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
		QImage requestImage(const QString &id, QSize *size, const QSize &requested_size) override;
};

class FarsideBackend : public QObject
{
	Q_OBJECT
	// Farside Host is only ever "host" (usbip-win2 is installed, this PC receives a controller), Farside
	// Client only ever "client" (usbipd-win is installed, or this is Linux; this PC shares one).
	// "none": the USB/IP tool this program needs is not installed.
	Q_PROPERTY(QString role READ role CONSTANT)
	// Host: Not paired, Waiting for client, Client online, Linked. Client: Not paired, Connecting,
	// Host online, Linked, Host busy. Either: Stopped (the session could not start; see detail).
	Q_PROPERTY(QString state READ state NOTIFY changed)
	Q_PROPERTY(QString detail READ detail NOTIFY changed)
	// Host: the Client that is connected now. Client: the Host this PC is paired with, empty if none.
	Q_PROPERTY(QString peerName READ peerName NOTIFY changed)
	Q_PROPERTY(QString bus READ bus NOTIFY changed)
	Q_PROPERTY(QString controller READ controller NOTIFY changed)
	Q_PROPERTY(int rttMs READ rttMs NOTIFY changed)
	// Host: every paired PC as { id, name, online, linked }.
	Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
	Q_PROPERTY(bool keepLinked READ keepLinked WRITE setKeepLinked NOTIFY keepLinkedChanged)
	// Direct connections (no tunnel). Host: the switch. Both: the number to compare while a direct
	// pairing waits. Client: the Hosts that answered the last lookups ({ id, name, address, port, pairing }).
	Q_PROPERTY(bool directEnabled READ directEnabled WRITE setDirectEnabled NOTIFY directEnabledChanged)
	Q_PROPERTY(QString pairCheck READ pairCheck NOTIFY pairCheckChanged)
	Q_PROPERTY(QVariantList foundHosts READ foundHosts NOTIFY foundHostsChanged)
	// Host: the pairing code on show, empty when there is none (not made yet, used, or run out).
	Q_PROPERTY(QString code READ code NOTIFY codeChanged)
	Q_PROPERTY(int codeSecondsLeft READ codeSecondsLeft NOTIFY codeChanged)
	// Host: the name of the PC that waits for Accept or Reject, empty when nobody does.
	Q_PROPERTY(QString pairRequest READ pairRequest NOTIFY pairRequestChanged)
	// Client: a pairing is under way.
	Q_PROPERTY(bool pairing READ pairing NOTIFY pairingChanged)
	Q_PROPERTY(bool logEnabled READ logEnabled WRITE setLogEnabled NOTIFY logEnabledChanged)
	// { usbip, usbipd, tunnel }: whether each helper program was found. Refreshed by refreshTools().
	Q_PROPERTY(QVariantMap tools READ tools NOTIFY toolsChanged)
	// This program's version, and the newer one that waits (empty: none) with how far it is:
	// "downloading", or "ready" (installUpdate() works now).
	Q_PROPERTY(QString version READ version CONSTANT)
	Q_PROPERTY(QString updateVersion READ updateVersion NOTIFY updateChanged)
	Q_PROPERTY(QString updateState READ updateState NOTIFY updateChanged)

	public:
		explicit FarsideBackend(QObject *parent = nullptr);
		~FarsideBackend();

		QString role() const { return role_; }
		QString state() const { return status.state; }
		QString detail() const { return status.detail; }
		QString peerName() const { return peer_name; }
		QString bus() const { return status.bus; }
		QString controller() const { return status.controller; }
		int rttMs() const { return status.rtt_ms; }
		QVariantList devices() const { return devices_; }
		bool keepLinked() const { return keep_linked; }
		void setKeepLinked(bool on);
		bool directEnabled() const { return direct_enabled; }
		void setDirectEnabled(bool on);
		QString pairCheck() const { return pair_check; }
		QVariantList foundHosts() const { return found_hosts; }
		QString code() const { return code_; }
		int codeSecondsLeft() const;
		QString pairRequest() const { return pair_request; }
		bool pairing() const { return pairing_; }
		bool logEnabled() const { return log_enabled; }
		void setLogEnabled(bool on);
		QVariantMap tools() const { return tools_; }
		QString version() const { return QStringLiteral(FARSIDE_VERSION); }
		QString updateVersion() const { return update.version; }
		QString updateState() const { return update_state; }

		Q_INVOKABLE void newCode();
		Q_INVOKABLE void answerPair(bool accept);
		// Host: forget that PC. Client: forget the Host (id is not used).
		Q_INVOKABLE void unpair(const QString &id = QString());
		// Client. The answer comes as pairFinished().
		Q_INVOKABLE void pair(const QString &code);
		// Client: pair with a Host by address, no tunnel. And: ask the network which Hosts are there.
		Q_INVOKABLE void pairDirect(const QString &address);
		Q_INVOKABLE void findHosts();
		// Host: detach the controller and stay detached (turns keepLinked off).
		Q_INVOKABLE void unlink();
		Q_INVOKABLE void refreshTools();
		Q_INVOKABLE void copyText(const QString &text);
		// Host: the QR picture of the code on show, to the clipboard.
		Q_INVOKABLE void copyQrImage();
		// Client: the pairing code in the clipboard (a QR picture such as a screenshot, or the text),
		// or on any screen of this PC. Empty when there is none.
		Q_INVOKABLE QString codeFromClipboard() const;
		Q_INVOKABLE QString codeFromScreen() const;
		Q_INVOKABLE void openFolder(const QString &folder);
		Q_INVOKABLE QString logFolder() const;
		// Asks GitHub for a newer release and fetches its Setup program. Runs by itself after the start
		// and every six hours; never restarts anything.
		Q_INVOKABLE void checkForUpdate();
		// Ends this program and starts the fetched Setup program, which installs and starts it again.
		Q_INVOKABLE void installUpdate();
		// Client: has usbipd-win share the DualSense that is plugged in. Windows asks for administrator rights.
		Q_INVOKABLE void shareController();

	signals:
		void changed();
		void keepLinkedChanged();
		void directEnabledChanged();
		void foundHostsChanged();
		void codeChanged();
		void pairRequestChanged();
		void pairCheckChanged();
		void pairingChanged();
		void logEnabledChanged();
		void toolsChanged();
		void updateChanged();
		// Empty text = paired. Otherwise a sentence for the user.
		void pairFinished(const QString &problem);

	private:
		void onLink(std::function<void()> work);
		void publish(); // link thread
		void log(const QString &line);
		void fetchUpdate(const Farside::Update &found);
		void setUpdate(const Farside::Update &found, const QString &state);

		QString role_;
		QThread thread;
		QObject *worker = nullptr;       // lives on the link thread
		Farside::Host *host = nullptr;   // made, used and deleted on the link thread only
		Farside::Client *client = nullptr;
		QString start_error;             // link thread

		Farside::Status status;
		QString peer_name, code_, pair_request, pair_check, last_logged;
		QVariantList devices_;
		QVariantMap tools_;
		QDateTime code_until;
		QTimer code_timer, ask_timer;
		bool keep_linked = true, pairing_ = false, log_enabled = false, direct_enabled = false;
		QVariantList found_hosts;
		QNetworkAccessManager net;
		QTimer update_timer;
		Farside::Update update;
		QString update_state, update_file;
};
