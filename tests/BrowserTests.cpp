#include "browser/BrowserTab.h"
#include "network/NetworkManager.h"
#include "profiles/Profile.h"
#include "ui/MainWindow.h"
#include <QLineEdit>
#include <QNetworkCookie>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebEngineCookieStore>
#include <QWebEngineView>
#include <QtTest>
class BrowserTests : public QObject {
    Q_OBJECT
    QTcpServer server;
    QTemporaryDir data;
    QUrl local(const QString &path = "/") const {
        return QUrl(QString("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path));
    }
  private slots:
    void initTestCase() {
        QVERIFY(data.isValid());
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                    const auto request = socket->readAll();
                    if (!request.contains("\r\n\r\n"))
                        return;
                    const QByteArray body =
                        "<!doctype html><title>Minato test</title><h1>Local navigation</h1><a "
                        "href='/second'>Next</a><a href='minato://network'>Forbidden</a>";
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nSet-Cookie: minato_test=ok; "
                                  "Max-Age=3600; Path=/\r\nContent-Length: " +
                                  QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    void guestAndNavigation() {
        Profile profile("test", true, data.path());
        QVERIFY(profile.web->isOffTheRecord());
        BrowserTab tab(profile);
        tab.resize(1000, 700);
        tab.show();
        QSignalSpy loaded(tab.view(), &QWebEngineView::loadFinished);
        tab.navigate(local());
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 20000);
        QVERIFY(loaded.last().at(0).toBool());
        QCOMPARE(tab.title(), QString("Minato test"));
        QTRY_COMPARE(profile.store->entries(false).size(), 1);
        tab.navigate(local("/second"));
        QTRY_COMPARE(tab.view()->url(), local("/second"));
        QTRY_VERIFY(tab.canBack());
        tab.back();
        QTRY_COMPARE(tab.view()->url(), local());
        tab.view()->page()->runJavaScript("location.href='minato://network'");
        QTest::qWait(300);
        QVERIFY(!tab.internal());
        QCOMPARE(tab.url(), local());
        tab.navigate(QUrl("minato://network"));
        QVERIFY(tab.internal());
        QVERIFY(tab.findChild<QTableWidget *>());
    }
    void persistentCookiesAndStorage() {
        {
            Profile profile("persistent", false, data.path());
            QVERIFY(!profile.web->isOffTheRecord());
            QCOMPARE(profile.web->persistentCookiesPolicy(), QWebEngineProfile::ForcePersistentCookies);
            BrowserTab tab(profile);
            QSignalSpy loaded(tab.view(), &QWebEngineView::loadFinished);
            tab.navigate(local());
            QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 20000);
            QVERIFY(loaded.last().at(0).toBool());
            auto result = std::make_shared<QVariant>();
            tab.view()->page()->runJavaScript(
                "localStorage.setItem('state','present'); document.cookie='saved_session=yes; path=/'; "
                "document.cookie='saved_persistent=yes; Max-Age=3600; path=/'; "
                "document.cookie.includes('saved_session=yes')",
                [result](const QVariant &value) { *result = value; });
            QTRY_VERIFY(result->isValid());
            QVERIFY(result->toBool());
        }
        {
            Profile profile("persistent", false, data.path());
            BrowserTab tab(profile);
            QSignalSpy loaded(tab.view(), &QWebEngineView::loadFinished);
            tab.navigate(local());
            QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 20000);
            QVERIFY(loaded.last().at(0).toBool());
            auto result = std::make_shared<QVariant>();
            tab.view()->page()->runJavaScript("document.cookie.includes('saved_session=yes') && "
                                              "document.cookie.includes('saved_persistent=yes') "
                                              "&& localStorage.getItem('state')==='present'",
                                              [result](const QVariant &value) { *result = value; });
            QTRY_VERIFY(result->isValid());
            QVERIFY2(result->toBool(), "Cookies and localStorage must survive profile reopening");
        }
        {
            Profile profile("isolated", false, data.path());
            BrowserTab tab(profile);
            QSignalSpy loaded(tab.view(), &QWebEngineView::loadFinished);
            tab.navigate(local());
            QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 20000);
            auto result = std::make_shared<QVariant>();
            tab.view()->page()->runJavaScript(
                "!document.cookie.includes('saved_session=') && localStorage.getItem('state')===null",
                [result](const QVariant &value) { *result = value; });
            QTRY_VERIFY(result->isValid());
            QVERIFY2(result->toBool(), "Different profiles must not share Web storage");
        }
    }
    void persistenceAcrossProcesses() {
        for (const auto &mode : QStringList{"write", "read"}) {
            QProcess process;
            auto environment = QProcessEnvironment::systemEnvironment();
            environment.insert("MINATO_PROBE_MODE", mode);
            environment.insert("MINATO_PROBE_ROOT", data.path());
            process.setProcessEnvironment(environment);
            process.start(QCoreApplication::applicationFilePath(), QStringList{});
            QVERIFY(process.waitForStarted());
            QTRY_VERIFY_WITH_TIMEOUT(process.state() == QProcess::NotRunning, 30000);
            QCOMPARE(process.exitStatus(), QProcess::NormalExit);
            QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        }
    }
    void publicIpFailureAndTimeout() {
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost));
        NetworkManager manager;
        manager.setProvider(QUrl(QString("https://127.0.0.1:%1").arg(silent.serverPort())));
        QSignalSpy result(&manager, &NetworkManager::publicIpReady);
        QElapsedTimer timer;
        timer.start();
        manager.fetchPublicIp();
        QVERIFY(timer.elapsed() < 500);
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 15000);
        QVERIFY(result.first().first().toString().contains("indisponible"));
        QVERIFY(timer.elapsed() < 14000);
    }
    void tabsAndPages() {
        Profile profile("ui", true, data.path());
        MainWindow window(profile);
        window.show();
        QTest::qWait(100);
        auto *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 1);
        window.addTab(QUrl("minato://history"));
        QCOMPARE(tabs->count(), 2);
        for (const auto &page :
             QStringList{"newtab", "settings", "network", "history", "bookmarks", "about", "passwords"}) {
            auto *tab = qobject_cast<BrowserTab *>(tabs->currentWidget());
            tab->navigate(QUrl("minato://" + page));
            QVERIFY(tab->internal());
        }
        window.activateWindow();
        window.raise();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        window.findChild<QLineEdit *>("addressBar")->setFocus();
        QTest::keyClick(&window, Qt::Key_W, Qt::ControlModifier);
        QTRY_COMPARE(tabs->count(), 1);
        QTest::keyClick(&window, Qt::Key_T, Qt::ControlModifier);
        QTRY_COMPARE(tabs->count(), 2);
        QTest::keyClick(&window, Qt::Key_W, Qt::ControlModifier);
        QTRY_COMPARE(tabs->count(), 1);
        QTest::qWait(200);
        QCOMPARE(window.findChild<QLineEdit *>("addressBar")->text(), QString("minato://newtab"));
        window.findChild<QLineEdit *>("addressBar")->clearFocus();
        QVERIFY(window.grab().save(data.filePath("newtab.png")));
        if (!qEnvironmentVariableIsEmpty("MINATO_SCREENSHOT_DIR"))
            window.grab().save(qEnvironmentVariable("MINATO_SCREENSHOT_DIR") + "/newtab.png");
        window.addTab(QUrl("minato://network"));
        QCOMPARE(window.findChild<QLineEdit *>("addressBar")->text(), QString("minato://network"));
        QTest::qWait(200);
        if (!qEnvironmentVariableIsEmpty("MINATO_SCREENSHOT_DIR"))
            window.grab().save(qEnvironmentVariable("MINATO_SCREENSHOT_DIR") + "/network.png");
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    const auto mode = qEnvironmentVariable("MINATO_PROBE_MODE");
    if (!mode.isEmpty()) {
        Profile profile("restart", false, qEnvironmentVariable("MINATO_PROBE_ROOT"));
        QWebEnginePage page(profile.web.get());
        QObject::connect(&page, &QWebEnginePage::loadFinished, &app, [&page, &app, mode](bool ok) {
            if (!ok) {
                app.exit(2);
                return;
            }
            const QString script =
                mode == "write"
                    ? "localStorage.setItem('restart','yes'); document.cookie='restart_session=yes; path=/'; "
                      "document.cookie.includes('restart_session=yes')"
                    : "localStorage.getItem('restart')==='yes' && "
                      "document.cookie.includes('restart_session=yes')";
            page.runJavaScript(script, [&app](const QVariant &result) { app.exit(result.toBool() ? 0 : 3); });
        });
        QTimer::singleShot(25000, &app, [&app] { app.exit(4); });
        page.setHtml("<!doctype html><title>Persistence probe</title>", QUrl("https://minato.invalid/"));
        return app.exec();
    }
    BrowserTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "BrowserTests.moc"
