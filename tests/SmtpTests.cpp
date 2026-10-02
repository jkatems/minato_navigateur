#include "reports/SmtpClient.h"
#include <QFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QTcpServer>
#include <QtTest>

// Only loopback traffic and dummy credentials; this server never forwards mail.
class FakeSmtp : public QTcpServer {
  public:
    bool startTls = false;
    bool offerTls = true;
    bool rejectAuth = false;
    QByteArray message;
    QList<QByteArray> commands;
    QSslCertificate certificate;
    QSslKey key;
    FakeSmtp() {
        QFile cert(QStringLiteral(SMTP_FIXTURE_DIR "/smtp-test-cert.pem"));
        if (!cert.open(QIODevice::ReadOnly))
            return;
        certificate = QSslCertificate(cert.readAll());
        QFile privateKey(QStringLiteral(SMTP_FIXTURE_DIR "/smtp-test-key.pem"));
        if (!privateKey.open(QIODevice::ReadOnly))
            return;
        key = QSslKey(privateKey.readAll(), QSsl::Rsa);
    }

  protected:
    void incomingConnection(qintptr descriptor) override {
        auto *socket = new QSslSocket(this);
        socket->setLocalCertificate(certificate);
        socket->setPrivateKey(key);
        socket->setPeerVerifyMode(QSslSocket::VerifyNone); // Server does not require a client certificate.
        socket->setSocketDescriptor(descriptor);
        auto input = std::make_shared<QByteArray>();
        auto phase = std::make_shared<int>(0);
        connect(socket, &QSslSocket::readyRead, socket, [this, socket, input, phase] {
            *input += socket->readAll();
            while (input->contains("\r\n")) {
                const auto end = input->indexOf("\r\n");
                const auto line = input->left(end);
                input->remove(0, end + 2);
                if (*phase == 3) {
                    if (line == ".") {
                        *phase = 4;
                        socket->write("250 queued\r\n");
                    } else
                        message += line + "\r\n";
                    continue;
                }
                commands << line;
                if (*phase == 1) {
                    *phase = 2;
                    socket->write("334 UGFzc3dvcmQ6\r\n");
                } else if (*phase == 2) {
                    *phase = 0;
                    socket->write(rejectAuth ? "535 rejected\r\n" : "235 authenticated\r\n");
                } else if (line.startsWith("EHLO")) {
                    if (startTls && !socket->isEncrypted())
                        socket->write(offerTls ? "250-local\r\n250 STARTTLS\r\n" : "250 local\r\n");
                    else
                        socket->write("250-local\r\n250 AUTH LOGIN\r\n");
                } else if (line == "STARTTLS") {
                    socket->write("220 ready\r\n");
                    socket->flush();
                    socket->startServerEncryption();
                } else if (line == "AUTH LOGIN") {
                    *phase = 1;
                    socket->write("334 VXNlcm5hbWU6\r\n");
                } else if (line.startsWith("MAIL FROM") || line.startsWith("RCPT TO"))
                    socket->write("250 ok\r\n");
                else if (line == "DATA") {
                    *phase = 3;
                    socket->write("354 send data\r\n");
                } else if (line == "QUIT")
                    socket->disconnectFromHost();
            }
        });
        if (startTls)
            socket->write("220 test server\r\n");
        else {
            connect(socket, &QSslSocket::encrypted, socket,
                    [socket] { socket->write("220 test server\r\n"); });
            socket->startServerEncryption();
        }
    }
};
class ScopedTrust {
    QSslConfiguration previous = QSslConfiguration::defaultConfiguration();

  public:
    explicit ScopedTrust(const QSslCertificate &certificate) {
        auto config = previous;
        auto ca = config.caCertificates();
        ca.append(certificate);
        config.setCaCertificates(ca);
        QSslConfiguration::setDefaultConfiguration(config);
    }
    ~ScopedTrust() {
        QSslConfiguration::setDefaultConfiguration(previous);
    }
};
class SmtpTests : public QObject {
    Q_OBJECT
    SmtpClient::Config settings(const FakeSmtp &server) {
        SmtpClient::Config config;
        config.host = "127.0.0.1";
        config.port = server.serverPort();
        config.sender = "sender@example.com";
        config.username = "test-user";
        config.password = "test-password";
        config.security = server.startTls ? SmtpClient::Security::StartTls : SmtpClient::Security::Tls;
        return config;
    }
  private slots:
    void initTestCase() {
        if (!QSslSocket::availableBackends().contains("openssl"))
            QSKIP("TLS test server requires the OpenSSL backend; production supports native TLS clients.");
        QVERIFY(QSslSocket::setActiveBackend("openssl"));
    }
    void successfulDelivery_data() {
        QTest::addColumn<bool>("startTls");
        QTest::newRow("implicit-TLS") << false;
        QTest::newRow("STARTTLS") << true;
    }
    void successfulDelivery() {
        QFETCH(bool, startTls);
        FakeSmtp server;
        server.startTls = startTls;
        QVERIFY(!server.certificate.isNull());
        QVERIFY(server.listen(QHostAddress::LocalHost));
        ScopedTrust trust(server.certificate);
        SmtpClient client;
        QSignalSpy done(&client, &SmtpClient::completed), error(&client, &SmtpClient::failed);
        const QByteArray body = "Rapport de test\nHistorique synthétique";
        client.send(settings(server), body);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
        QCOMPARE(error.count(), 0);
        QVERIFY(server.message.contains("To: <jkatemskatema@gmail.com>"));
        const auto encoded = server.message.mid(server.message.indexOf("\r\n\r\n") + 4);
        QCOMPARE(QByteArray::fromBase64(encoded), body);
        QVERIFY(!server.message.contains("test-password"));
        QVERIFY(!client.isActive());
    }
    void refusesUntrustedCertificate() {
        FakeSmtp server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        SmtpClient client;
        QSignalSpy error(&client, &SmtpClient::failed);
        client.send(settings(server), "test report");
        QTRY_COMPARE_WITH_TIMEOUT(error.count(), 1, 10000);
        QVERIFY(server.commands.isEmpty());
        QVERIFY(server.message.isEmpty());
    }
    void requiresStartTls() {
        FakeSmtp server;
        server.startTls = true;
        server.offerTls = false;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        SmtpClient client;
        QSignalSpy error(&client, &SmtpClient::failed);
        client.send(settings(server), "test report");
        QTRY_COMPARE_WITH_TIMEOUT(error.count(), 1, 5000);
        QVERIFY(!server.commands.contains("AUTH LOGIN"));
        QVERIFY(server.message.isEmpty());
    }
    void rejectedCredentials() {
        FakeSmtp server;
        server.rejectAuth = true;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        ScopedTrust trust(server.certificate);
        SmtpClient client;
        QSignalSpy error(&client, &SmtpClient::failed);
        client.send(settings(server), "test report");
        QTRY_COMPARE_WITH_TIMEOUT(error.count(), 1, 10000);
        QVERIFY(server.message.isEmpty());
        QVERIFY(!error.first().first().toString().contains("test-password"));
    }
    void invalidHeadersAndCancellation() {
        QVERIFY(!SmtpClient::validEmail("a@example.com\r\nBcc: b@example.com"));
        FakeSmtp server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        SmtpClient client;
        QSignalSpy error(&client, &SmtpClient::failed);
        client.send(settings(server), "test report");
        client.cancel();
        QCOMPARE(error.count(), 1);
        QVERIFY(!client.isActive());
        QVERIFY(server.message.isEmpty());
    }
};
QTEST_GUILESS_MAIN(SmtpTests)
#include "SmtpTests.moc"
