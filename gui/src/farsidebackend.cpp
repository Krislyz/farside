// See farsidebackend.h.
#include <farsidebackend.h>
#include <farsideqr.h>

#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QMimeData>
#include <QNetworkReply>
#include <QPixmap>
#include <QScreen>
#include <QSettings>
#include <QUrl>

#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

using Farside::findTool;

static QString sandbox() { return qEnvironmentVariable("FARSIDE_SANDBOX"); }

static QString detectRole()
{
	// Farside Client only ever shares a controller, Farside Host only ever receives one.
#ifdef FARSIDE_CLIENT_EDITION
	const QString mine = QStringLiteral("client");
#ifdef Q_OS_WIN
	const bool tool = !findTool(QStringLiteral("usbipd"), QStringLiteral("usbipd-win")).isEmpty();
#else
	const bool tool = true; // the kernel's usbip-host; see read_bus in farsidesession.cpp
#endif
#else
	const QString mine = QStringLiteral("host");
#ifdef Q_OS_WIN
	const bool tool = !findTool(QStringLiteral("usbip"), QStringLiteral("USBip")).isEmpty();
#else
	// ponytail: receiving on Linux needs the attach and port commands of the Linux usbip tool in Farside::Host.
	const bool tool = false;
#endif
#endif
	// FARSIDE_ROLE (tests) skips the look at the tool; it cannot turn one program into the other.
	return tool || qEnvironmentVariable("FARSIDE_ROLE") == mine ? mine : QStringLiteral("none");
}

QImage FarsideQrProvider::requestImage(const QString &id, QSize *size, const QSize &requested_size)
{
	const QImage picture = Farside::qrDraw(QUrl::fromPercentEncoding(id.toUtf8()), requested_size.width() > 0 ? requested_size.width() : 480);
	if(size)
		*size = picture.size();
	return picture;
}

FarsideBackend::FarsideBackend(QObject *parent) : QObject(parent), role_(detectRole())
{
	// The typefaces of the Farside pages (Inter, Space Grotesk; SIL Open Font License).
	for(const char *file : { ":/fonts/Inter-Regular.ttf", ":/fonts/Inter-SemiBold.ttf", ":/fonts/SpaceGrotesk-SemiBold.ttf" })
		QFontDatabase::addApplicationFont(QString::fromLatin1(file));
	QSettings settings;
	keep_linked = settings.value(QStringLiteral("farside/keepLinked"), true).toBool();
	// A sandboxed test run starts with it off and never writes it: the stored switch belongs to the real Host.
	direct_enabled = sandbox().isEmpty() && settings.value(QStringLiteral("farside/direct"), false).toBool();
	log_enabled = settings.value(QStringLiteral("farside/log"), false).toBool();
	status.state = role_ == QLatin1String("none") ? QStringLiteral("Stopped") : QStringLiteral("Not paired");
	refreshTools();

	// Updates: a look at GitHub shortly after the start and every six hours. Not in a sandboxed test run,
	// and not when the stored switch farside/updates is false.
	update_timer.setInterval(6 * 60 * 60 * 1000);
	connect(&update_timer, &QTimer::timeout, this, [this] { checkForUpdate(); });
	if(sandbox().isEmpty() && settings.value(QStringLiteral("farside/updates"), true).toBool())
	{
		update_timer.start();
		QTimer::singleShot(10000, this, [this] { checkForUpdate(); });
	}

	code_timer.setInterval(1000);
	connect(&code_timer, &QTimer::timeout, this, [this] {
		if(codeSecondsLeft() == 0)
		{
			code_.clear();
			code_timer.stop();
		}
		emit codeChanged();
	});
	// The Host stops waiting for an answer after 60 seconds; so does the question on screen.
	ask_timer.setSingleShot(true);
	ask_timer.setInterval(60000);
	connect(&ask_timer, &QTimer::timeout, this, [this] { pair_request.clear(); emit pairRequestChanged(); });

	worker = new QObject;
	worker->moveToThread(&thread);
	thread.setObjectName(QStringLiteral("farside-link"));
	thread.start();

	const QString box = sandbox();
	const int shift = box.isEmpty() ? 0 : 100;
	const bool keep = keep_linked, direct = direct_enabled;
	onLink([this, box, shift, keep, direct] {
		if(role_ == QLatin1String("host"))
		{
			host = new Farside::Host(worker);
			if(!box.isEmpty())
				host->store.path = QDir(box).filePath(QStringLiteral("link.dat"));
			host->host_port += shift;
			host->usb_port += shift;
			host->lan_port += shift;
			host->lookup_port += shift;
			host->direct = direct;
			// A sandboxed copy stays on this PC: tests must not make Windows ask about the firewall.
			if(!box.isEmpty())
				host->lan_address = QHostAddress::LocalHost;
			if(!box.isEmpty())
			{
				// The USB/IP driver is one per PC: a sandboxed Host that asked it "is a controller attached?"
				// would see the real Farside's controller and detach it. So the sandbox never touches the driver.
				host->is_attached = [](std::function<void(bool)> done) { done(false); };
				host->attach = [](const QString &, std::function<void(bool, QString)> done) { done(false, QStringLiteral("Sandbox: no controller is attached.")); };
				host->detach = [] {};
			}
			host->keep_linked = keep;
			connect(host, &Farside::Host::statusChanged, worker, [this] { publish(); });
			connect(host, &Farside::Host::pairRequested, this, [this](const QString &name, const QString &check) {
				pair_check = check;
				emit pairCheckChanged();
				pair_request = name;
				ask_timer.start();
				emit pairRequestChanged();
			});
			if(!host->start(&start_error))
				publish();
		}
		else if(role_ == QLatin1String("client"))
		{
			client = new Farside::Client(worker);
			if(!box.isEmpty())
				client->store.path = QDir(box).filePath(QStringLiteral("link.dat"));
			client->client_port += shift;
			client->lan_port += shift;
			client->lookup_port += shift;
			if(!box.isEmpty())
				client->lookup_bind = QHostAddress::LocalHost;
			connect(client, &Farside::Client::pairCheck, this, [this](const QString &number) {
				pair_check = number;
				emit pairCheckChanged();
			});
			connect(client, &Farside::Client::hostFound, this, [this](const QString &id, const QString &name, const QString &address, int port, bool open) {
				const QVariantMap entry { { QStringLiteral("id"), id }, { QStringLiteral("name"), name }, { QStringLiteral("address"), address },
					{ QStringLiteral("port"), port }, { QStringLiteral("pairing"), open }, { QStringLiteral("seen"), QDateTime::currentMSecsSinceEpoch() } };
				for(int i = 0; i < found_hosts.size(); i++)
					if(found_hosts[i].toMap().value(QStringLiteral("id")) == id)
					{
						found_hosts[i] = entry;
						emit foundHostsChanged();
						return;
					}
				found_hosts.append(entry);
				emit foundHostsChanged();
			});
			connect(client, &Farside::Client::statusChanged, worker, [this] { publish(); });
			connect(client, &Farside::Client::pairFinished, this, [this](const QString &problem) {
				pairing_ = false;
				emit pairingChanged();
				pair_check.clear();
				emit pairCheckChanged();
				emit pairFinished(problem);
			});
			client->start();
			publish();
		}
	});
}

FarsideBackend::~FarsideBackend()
{
	// The sessions own sockets and timers of the link thread, so they end there too.
	QMetaObject::invokeMethod(worker, [this] {
		delete host;
		delete client;
		host = nullptr;
		client = nullptr;
	}, Qt::BlockingQueuedConnection);
	thread.quit();
	thread.wait();
	delete worker;
}

void FarsideBackend::onLink(std::function<void()> work)
{
	QMetaObject::invokeMethod(worker, std::move(work), Qt::QueuedConnection);
}

// Link thread: copy what the pages show and hand the copy to the window's thread.
void FarsideBackend::publish()
{
	Farside::Status s;
	QVariantList devices;
	QString name;
	if(host)
	{
		s = host->status();
		name = s.peer_name;
		for(const Farside::Peer &p : host->store.peers)
		{
			const bool online = p.id == s.peer_id;
			devices.append(QVariantMap {
				{ QStringLiteral("id"), p.id }, { QStringLiteral("name"), p.name },
				{ QStringLiteral("online"), online }, { QStringLiteral("linked"), online && s.state == QLatin1String("Linked") } });
		}
	}
	else if(client)
	{
		s = client->status();
		if(!client->store.host_ticket.isEmpty())
			name = client->store.host_name;
	}
	if(!start_error.isEmpty())
	{
		s.state = QStringLiteral("Stopped");
		s.detail = start_error;
	}
	QMetaObject::invokeMethod(this, [this, s, devices, name] {
		status = s;
		devices_ = devices;
		peer_name = name;
		emit changed();
		const QString line = QStringLiteral("%1 | peer %2 | bus %3 | %4").arg(s.state, name.isEmpty() ? QStringLiteral("-") : name,
			s.bus.isEmpty() ? QStringLiteral("-") : s.bus, s.detail);
		if(line != last_logged)
		{
			last_logged = line;
			log(line);
		}
	}, Qt::QueuedConnection);
}

QString FarsideBackend::logFolder() const
{
	return sandbox().isEmpty() ? QFileInfo(Farside::Store::defaultPath()).absolutePath() : sandbox();
}

// ponytail: one file that only grows, a line per change of state. Cut it by size if people leave it on.
void FarsideBackend::log(const QString &line)
{
	if(!log_enabled)
		return;
	QDir().mkpath(logFolder());
	QFile file(QDir(logFolder()).filePath(QStringLiteral("farside.log")));
	if(file.open(QIODevice::Append | QIODevice::Text))
		file.write((QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char(' ') + line + QLatin1Char('\n')).toUtf8());
}

void FarsideBackend::setLogEnabled(bool on)
{
	if(on == log_enabled)
		return;
	log_enabled = on;
	QSettings().setValue(QStringLiteral("farside/log"), on);
	emit logEnabledChanged();
	log(on ? QStringLiteral("log on | ") + last_logged : QString());
}

void FarsideBackend::setKeepLinked(bool on)
{
	if(on == keep_linked)
		return;
	keep_linked = on;
	QSettings().setValue(QStringLiteral("farside/keepLinked"), on);
	emit keepLinkedChanged();
	onLink([this, on] { if(host) host->keep_linked = on; });
}

void FarsideBackend::setDirectEnabled(bool on)
{
	if(on == direct_enabled)
		return;
	direct_enabled = on;
	if(sandbox().isEmpty())
		QSettings().setValue(QStringLiteral("farside/direct"), on);
	emit directEnabledChanged();
	onLink([this, on] { if(host) host->setDirect(on); });
}

void FarsideBackend::pairDirect(const QString &address)
{
	if(pairing_ || role_ != QLatin1String("client"))
		return;
	pairing_ = true;
	emit pairingChanged();
	onLink([this, address] { if(client) client->pairDirect(address); });
}

void FarsideBackend::findHosts()
{
	// A Host that stopped answering leaves the list after 7 seconds.
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	bool dropped = false;
	for(int i = found_hosts.size() - 1; i >= 0; i--)
		if(now - found_hosts[i].toMap().value(QStringLiteral("seen")).toLongLong() > 7000)
		{
			found_hosts.removeAt(i);
			dropped = true;
		}
	if(dropped)
		emit foundHostsChanged();
	onLink([this] { if(client) client->lookup(); });
}

void FarsideBackend::unlink()
{
	setKeepLinked(false);
	onLink([this] { if(host && host->detach) host->detach(); });
}

int FarsideBackend::codeSecondsLeft() const
{
	return code_.isEmpty() ? 0 : int(qMax<qint64>(0, QDateTime::currentDateTimeUtc().secsTo(code_until)));
}

void FarsideBackend::newCode()
{
	onLink([this] {
		// Empty while the tunnel is still starting.
		const QString made = host ? host->beginPairing() : QString();
		// Sandbox only: a test that plays the other PC reads the code from here instead of from the screen.
		if(QFile file(QDir(sandbox()).filePath(QStringLiteral("code.txt"))); !sandbox().isEmpty() && file.open(QIODevice::WriteOnly | QIODevice::Truncate))
			file.write(made.toUtf8());
		QMetaObject::invokeMethod(this, [this, made] {
			code_ = made;
			code_until = QDateTime::currentDateTimeUtc().addSecs(600);
			if(made.isEmpty())
				code_timer.stop();
			else
				code_timer.start();
			emit codeChanged();
		}, Qt::QueuedConnection);
	});
}

void FarsideBackend::answerPair(bool accept)
{
	if(pair_request.isEmpty())
		return;
	pair_request.clear();
	pair_check.clear();
	emit pairCheckChanged();
	ask_timer.stop();
	emit pairRequestChanged();
	if(accept)
	{
		// A code works once.
		code_.clear();
		code_timer.stop();
		emit codeChanged();
	}
	onLink([this, accept] {
		if(!host)
			return;
		host->answerPair(accept);
		publish();
	});
}

void FarsideBackend::unpair(const QString &id)
{
	onLink([this, id] {
		if(host)
			host->unpair(id);
		else if(client)
			client->unpair();
		publish();
	});
}

void FarsideBackend::pair(const QString &code)
{
	if(pairing_ || role_ != QLatin1String("client"))
		return;
	pairing_ = true;
	emit pairingChanged();
	onLink([this, code] { if(client) client->pair(code); });
}

void FarsideBackend::refreshTools()
{
	tools_ = QVariantMap {
		{ QStringLiteral("usbip"), !findTool(QStringLiteral("usbip"), QStringLiteral("USBip")).isEmpty() },
		{ QStringLiteral("usbipd"), !findTool(QStringLiteral("usbipd"), QStringLiteral("usbipd-win")).isEmpty() },
		{ QStringLiteral("tunnel"), !findTool(QStringLiteral("dumbpipe")).isEmpty() } };
	emit toolsChanged();
}

void FarsideBackend::copyText(const QString &text)
{
	QGuiApplication::clipboard()->setText(text);
}

void FarsideBackend::copyQrImage()
{
	const QImage picture = Farside::qrDraw(code_, 600);
	if(!picture.isNull())
		QGuiApplication::clipboard()->setImage(picture);
}

QString FarsideBackend::codeFromClipboard() const
{
	const QClipboard *clipboard = QGuiApplication::clipboard();
	if(const QMimeData *data = clipboard->mimeData(); data && data->hasImage())
		if(const QString code = Farside::qrRead(clipboard->image()); !code.isEmpty())
			return code;
	const QString text = clipboard->text().trimmed().toLower();
	QString ticket;
	QByteArray secret;
	return Farside::parseCode(text, &ticket, &secret) ? text : QString();
}

QString FarsideBackend::codeFromScreen() const
{
	for(QScreen *screen : QGuiApplication::screens())
		if(const QString code = Farside::qrRead(screen->grabWindow(0).toImage()); !code.isEmpty())
			return code;
	return QString();
}

void FarsideBackend::openFolder(const QString &folder)
{
	QDir().mkpath(folder);
	QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void FarsideBackend::setUpdate(const Farside::Update &found, const QString &state)
{
	update = found;
	update_state = state;
	emit updateChanged();
}

static QByteArray fileSha256(const QString &path)
{
	QFile file(path);
	QCryptographicHash hash(QCryptographicHash::Sha256);
	return file.open(QIODevice::ReadOnly) && hash.addData(&file) ? hash.result() : QByteArray();
}

void FarsideBackend::checkForUpdate(bool by_user)
{
#ifdef Q_OS_WIN
	// A sandboxed test run asks GitHub only when the check is asked for.
	if((!sandbox().isEmpty() && !by_user) || !update_state.isEmpty())
		return;
	const QString waiting = by_user ? QStringLiteral("checking") : QString();
	if(by_user)
		setUpdate({}, waiting);
	// What a check the user asked for ended with, shown for six seconds.
	auto answerUser = [this, by_user](const QString &outcome) {
		if(!by_user)
			return;
		setUpdate({}, outcome);
		QTimer::singleShot(6000, this, [this, outcome] { if(update_state == outcome) setUpdate({}, QString()); });
	};
	QNetworkRequest ask(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QLatin1String(Farside::update_repo))));
	ask.setRawHeader("Accept", "application/vnd.github+json");
	ask.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Farside"));
	ask.setTransferTimeout(15000);
	QNetworkReply *answer = net.get(ask);
	connect(answer, &QNetworkReply::finished, this, [this, answer, waiting, answerUser] {
		answer->deleteLater();
		// Another check got there first (an update is on its way, or the user's own check took over).
		if(update_state != waiting)
			return;
		// Offline, or GitHub is busy: the next look is in six hours or at the next start.
		if(answer->error() != QNetworkReply::NoError)
			return answerUser(QStringLiteral("failed"));
		const Farside::Update found = Farside::pickUpdate(answer->readAll(), version(), QStringLiteral(FARSIDE_SETUP_NAME));
		if(found.version.isEmpty())
			return answerUser(QStringLiteral("current"));
		fetchUpdate(found);
	});
#else
	Q_UNUSED(by_user);
#endif
}

void FarsideBackend::fetchUpdate(const Farside::Update &found)
{
	const QString folder = QDir::temp().filePath(QStringLiteral("Farside-update"));
	QDir().mkpath(folder);
	auto file = std::make_shared<QFile>(QDir(folder).filePath(QStringLiteral(FARSIDE_SETUP_NAME)));
	if(!file->open(QIODevice::WriteOnly | QIODevice::Truncate))
		return;
	setUpdate(found, QStringLiteral("downloading"));
	QNetworkRequest ask { QUrl(found.url) };
	ask.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Farside"));
	QNetworkReply *answer = net.get(ask);
	connect(answer, &QNetworkReply::readyRead, this, [answer, file] { file->write(answer->readAll()); });
	connect(answer, &QNetworkReply::finished, this, [this, answer, file, found] {
		answer->deleteLater();
		file->write(answer->readAll());
		file->close();
		// Trust boundary: this file gets run. It must be, byte for byte, the one GitHub lists for the release.
		if(answer->error() != QNetworkReply::NoError || file->size() != found.size || fileSha256(file->fileName()) != found.sha256)
		{
			file->remove();
			setUpdate({}, QString()); // tried again at the next look
			return;
		}
		update_file = file->fileName();
		setUpdate(found, QStringLiteral("ready"));
		log(QStringLiteral("update %1 fetched").arg(found.version));
	});
}

void FarsideBackend::installUpdate()
{
#ifdef Q_OS_WIN
	if(update_state != QLatin1String("ready"))
		return;
	// The file sat in the temporary folder since it was checked: check it again right before it runs.
	if(fileSha256(update_file) != update.sha256)
	{
		QFile::remove(update_file);
		setUpdate({}, QString());
		return;
	}
	// Setup waits until this program has ended, installs, and starts the program again (see farside/installer).
	const QString file = QDir::toNativeSeparators(update_file);
	if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(file.utf16()), L"/SILENT", nullptr, SW_SHOWNORMAL)) > 32)
		QCoreApplication::quit();
#endif
}

void FarsideBackend::shareController()
{
#ifdef Q_OS_WIN
	const QString usbipd = QDir::toNativeSeparators(findTool(QStringLiteral("usbipd"), QStringLiteral("usbipd-win")));
	if(usbipd.isEmpty())
		return;
	// DualSense (054c:0ce6), then DualSense Edge (054c:0df2); the one that is not plugged in just fails.
	// "runas": sharing a device needs administrator rights, so Windows asks.
	const QString args = QStringLiteral("/c \"\"%1\" bind --force --hardware-id 054c:0ce6 & \"%1\" bind --force --hardware-id 054c:0df2\"").arg(usbipd);
	ShellExecuteW(nullptr, L"runas", L"cmd.exe", reinterpret_cast<LPCWSTR>(args.utf16()), nullptr, SW_HIDE);
#endif
}
