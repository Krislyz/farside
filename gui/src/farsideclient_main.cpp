// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Farside Remote ("client" in the code): pairs with a Farside and shares this PC's controller with it. That is all it does:
// the stream, the PS5 and everything of the receiving side belong to Farside Host (main.cpp).
#include <farsidebackend.h>
#include <farsidelink.h>
#include <farsideqr.h>
#include <farsidesession.h>
#include <farsideupdate.h>

#include <QGuiApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
	// The self-test of everything this program is made of (0 = pass, otherwise the first failing check).
	for(int i = 1; i < argc; i++)
		if(!strcmp(argv[i], "--farside-selftest"))
		{
			QCoreApplication selftest_app(argc, argv);
			if(const int core = Farside::selfTest())
				return core;
			if(const int sessions = Farside::selfTestSessions())
				return sessions;
			if(const int direct = Farside::selfTestDirect())
				return direct;
			if(const int update = Farside::selfTestUpdate())
				return update;
			return Farside::selfTestQr();
		}

	// The settings names of Farside Host, so the switches of an earlier build (one program for both) are kept.
	QGuiApplication::setOrganizationName(QStringLiteral("Chiaki"));
	QGuiApplication::setApplicationName(QStringLiteral("Chiaki"));
	QGuiApplication::setApplicationVersion(QStringLiteral(FARSIDE_VERSION));
	QGuiApplication app(argc, argv);
	QGuiApplication::setApplicationDisplayName(QStringLiteral("Farside Remote"));
	QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/farside.svg")));
#ifdef Q_OS_WIN
	// Setup looks for this name and waits for the program to end before it replaces the files.
	CreateMutexW(nullptr, FALSE, L"FarsideClientRunning");
#endif

	qmlRegisterSingletonInstance("org.streetpea.chiaking", 1, 0, "Farside", new FarsideBackend(&app));
	QQmlApplicationEngine engine;
	// The collected program has Qt's QML modules in "qml" beside it (as Farside Host does, see main.cpp).
	engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
	engine.addImageProvider(QStringLiteral("farsideqr"), new FarsideQrProvider);
	engine.load(QUrl(QStringLiteral("qrc:/FarsideClientMain.qml")));
	if(engine.rootObjects().isEmpty())
		return 1;

	// For checking the pages where no window may be shown (a sandboxed run only, see farsidebackend.h):
	// FARSIDE_SHOT=<file.png> saves a picture of the window after 3 seconds (or FARSIDE_SHOT_MS) and ends
	// the program; FARSIDE_SHOT_TAB=<0|1|2> picks the tab first; FARSIDE_SHOT_CLICK=<x>,<y> clicks at that
	// point of the window after 1 second.
	if(const QString shot = qEnvironmentVariable("FARSIDE_SHOT"); !shot.isEmpty() && qEnvironmentVariableIsSet("FARSIDE_SANDBOX"))
	{
		auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
		window->setProperty("tab", qEnvironmentVariableIntValue("FARSIDE_SHOT_TAB"));
		const QStringList at = qEnvironmentVariable("FARSIDE_SHOT_CLICK").split(QLatin1Char(','));
		if(at.size() == 2)
			QTimer::singleShot(1000, &app, [window, at] {
				const QPointF point(at[0].toDouble(), at[1].toDouble());
				for(const QEvent::Type type : { QEvent::MouseButtonPress, QEvent::MouseButtonRelease })
				{
					QMouseEvent click(type, point, window->mapToGlobal(point), Qt::LeftButton,
						type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
					QCoreApplication::sendEvent(window, &click);
				}
			});
		const int shot_ms = qEnvironmentVariableIntValue("FARSIDE_SHOT_MS");
		QTimer::singleShot(shot_ms > 0 ? shot_ms : 3000, &app, [window, shot] {
			window->grabWindow().save(shot);
			QCoreApplication::quit();
		});
	}
	return app.exec();
}
