#include "sync/SyncClient.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>
class SyncTests : public QObject {
    Q_OBJECT
    QTcpServer server;
    QList<QJsonObject> received;
    QList<QByteArray> headers;
    int responseCode = 200;
    QUrl base() const {
        return QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));
    }
    const QString token = QString(43, 'a');
  private slots:
    void initTestCase() {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    QByteArray buffer = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", buffer);
                    const int split = buffer.indexOf("\r\n\r\n");
                    if (split < 0 || socket->property("handled").toBool())
                        return;
                    const auto head = buffer.left(split);
                    int length = 0;
                    for (const auto &line : head.split('\n'))
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(15).trimmed().toInt();
                    if (buffer.size() < split + 4 + length)
                        return;
                    socket->setProperty("handled", true);
                    headers << head;
                    const auto body = QJsonDocument::fromJson(buffer.mid(split + 4, length)).object();
                    received << body;
                    QJsonArray ids;
                    for (const auto &event : body.value("events").toArray())
                        ids.append(event.toObject().value("id"));
                    const QByteArray result = QJsonDocument(QJsonObject{{"accepted", ids}}).toJson();
                    socket->write(
                        "HTTP/1.1 " + QByteArray::number(responseCode) +
                        " Test\r\nContent-Type: application/json\r\nLocation: /stolen\r\nContent-Length: " +
                        QByteArray::number(result.size()) + "\r\nConnection: close\r\n\r\n" + result);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    void init() {
        received.clear();
        headers.clear();
        responseCode = 200;
    }
    void privateAndConsent() {
        SyncClient guest(true), regular(false);
        QVERIFY(!guest.start(base(), token, true));
        QVERIFY(!regular.start(base(), token, false));
        QVERIFY(!regular.start(QUrl("http://example.org"), token, true));
        QVERIFY(!regular.start(QUrl("https://example.org/api"), token, true));
        QVERIFY(!regular.start(base(), "bad\r\nheader", true));
        regular.record("visit", QUrl("https://example.org"), {}, {}, "test");
        QTest::qWait(400);
        QVERIFY(received.isEmpty());
    }
    void eventAndSanitization() {
        SyncClient client(false);
        QVERIFY(client.start(base(), token, true));
        client.recordInput("minato browser", QUrl("https://example.org/?q=minato&token=secret#hidden"),
                           "Travail");
        QTRY_COMPARE(received.size(), 1);
        auto event = received.first().value("events").toArray().first().toObject();
        QCOMPARE(event.value("kind").toString(), "search");
        QCOMPARE(event.value("query").toString(), "minato browser");
        QCOMPARE(event.value("url").toString(), "https://example.org/?q=minato");
        QVERIFY(event.contains("interfaces"));
        QCOMPARE(event.value("consent_version").toInt(), 1);
        QVERIFY(!event.contains("cookies"));
        QVERIFY(headers.first().contains("Authorization: Bearer " + token.toUtf8()));
        QVERIFY(!headers.first().toLower().contains("cookie:"));
        QTRY_VERIFY(client.status().contains("dernier envoi"));
        client.recordInput("example.org", QUrl("https://example.org"), "Travail");
        QTRY_COMPARE(received.size(), 2);
        QCOMPARE(received.last().value("events").toArray().first().toObject().value("kind").toString(),
                 "address");
    }
    void excludesInternalAndManagementServer() {
        SyncClient client(false);
        QVERIFY(client.start(base(), token, true));
        client.record("visit", QUrl("minato://network"), {}, {}, "test");
        client.record("visit", base().resolved(QUrl("/connexion/")), {}, {}, "test");
        client.record("visit", QUrl("https://user:pass@example.org"), {}, {}, "test");
        QTest::qWait(400);
        QVERIFY(received.isEmpty());
    }
    void retryKeepsEventId() {
        responseCode = 503;
        SyncClient client(false);
        QVERIFY(client.start(base(), token, true));
        client.record("visit", QUrl("https://example.org"), "Example", {}, "test");
        QTRY_COMPARE(received.size(), 1);
        QTRY_VERIFY(client.status().contains("indisponible"));
        responseCode = 200;
        QTRY_COMPARE_WITH_TIMEOUT(received.size(), 2, 7000);
        QCOMPARE(received.first(), received.last());
        QTRY_VERIFY(client.status().contains("dernier envoi"));
    }
    void rejectsRedirectAndRevocation() {
        for (const int code : {302, 401}) {
            received.clear();
            responseCode = code;
            SyncClient client(false);
            QVERIFY(client.start(base(), token, true));
            client.record("visit", QUrl("https://example.org"), {}, {}, "test");
            QTRY_COMPARE(received.size(), 1);
            QTRY_VERIFY(!client.active());
            QTest::qWait(300);
            QCOMPARE(received.size(), 1);
        }
    }
    void stopDropsPending() {
        SyncClient client(false);
        QVERIFY(client.start(base(), token, true));
        client.record("visit", QUrl("https://example.org"), {}, {}, "test");
        client.stop();
        QTest::qWait(400);
        QVERIFY(received.isEmpty());
    }
    void examModeIsLocalAndExcludesPrivate() {
        SyncClient guest(true), regular(false);
        QVERIFY(!guest.startExam());
        QVERIFY(regular.startExam());
        QVERIFY(regular.examMode());
        QCOMPARE(regular.server(), QUrl("http://127.0.0.1:8000"));
        regular.stop();
    }
    void examIntegration() {
        if (qEnvironmentVariable("MINATO_TEST_EXAM") != "1")
            QSKIP("Run through server/test_exam_integration.py with a disposable local database");
        SyncClient client(false);
        QVERIFY(client.startExam());
        client.record("visit", QUrl("https://example.org/examen"), "Examen integration", {}, "Examen");
        QTRY_VERIFY_WITH_TIMEOUT(client.status().contains("dernier envoi"), 10000);
    }
    void djangoIntegration() {
        const auto endpoint = qEnvironmentVariable("MINATO_TEST_SERVER");
        if (endpoint.isEmpty())
            QSKIP("Set MINATO_TEST_SERVER and MINATO_TEST_TOKEN for the Django integration test");
        SyncClient client(false);
        QVERIFY(client.start(QUrl(endpoint), qEnvironmentVariable("MINATO_TEST_TOKEN"), true));
        client.recordInput("integration minato", QUrl("https://example.org/?q=integration+minato"),
                           "Integration");
        QTRY_VERIFY_WITH_TIMEOUT(client.status().contains("dernier envoi"), 10000);
    }
};
QTEST_GUILESS_MAIN(SyncTests)
#include "SyncTests.moc"
