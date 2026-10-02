#include "SmtpClient.h"
#include "DiagnosticReport.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QUuid>

SmtpClient::SmtpClient(QObject *parent) : QObject(parent) {
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, this, [this] {
        fail(state == State::Accepted ? "Le serveur n’a pas confirmé la réception. Vérifiez la messagerie "
                                        "avant de réessayer pour éviter un doublon."
                                      : "Le serveur de messagerie ne répond pas dans le délai prévu.");
    });
    connect(&socket, &QSslSocket::readyRead, this, &SmtpClient::readResponse);
    connect(&socket, &QSslSocket::encrypted, this, [this] {
        if (state == State::StartTls)
            command("EHLO minato.local", State::SecureHello);
    });
    connect(&socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &) {
        fail("Le certificat TLS du serveur de messagerie n’est pas valide. Envoi refusé.");
    });
    connect(&socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        fail(state == State::Accepted
                 ? "Connexion interrompue avant confirmation. Vérifiez la messagerie avant de réessayer."
                 : "Connexion au serveur impossible ou interrompue. Vérifiez l’hôte, le port et votre "
                   "connexion.");
    });
    connect(&socket, &QSslSocket::disconnected, this, [this] {
        if (isActive())
            fail("Le serveur a fermé la connexion avant la confirmation d’envoi.");
    });
}

SmtpClient::~SmtpClient() {
    state = State::Idle;
    timeout.stop();
    socket.abort();
    clearSecrets();
}

bool SmtpClient::validEmail(const QString &email) {
    static const QRegularExpression pattern(QStringLiteral(
        "^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?\\.[A-Za-z]{2,63}$"));
    return email.size() <= 254 && pattern.match(email).hasMatch();
}
bool SmtpClient::isActive() const {
    return state != State::Idle;
}
void SmtpClient::send(const Config &settings, const QByteArray &report) {
    if (isActive())
        return;
    if (settings.host.trimmed().isEmpty() || settings.host.contains(QRegularExpression("[\\s/\\r\\n]")) ||
        settings.port == 0 || !validEmail(settings.sender) || settings.username.isEmpty() ||
        settings.password.isEmpty() || report.isEmpty() || report.size() > 4 * 1024 * 1024) {
        emit failed("Paramètres SMTP incomplets, adresse d’expéditeur invalide ou rapport trop volumineux "
                    "(maximum 4 Mo).");
        return;
    }
    if (!QSslSocket::supportsSsl()) {
        emit failed("Le support TLS n’est pas disponible dans cette installation Qt.");
        return;
    }
    config = settings;
    buffer.clear();
    response.clear();
    responseCode = 0;
    payload = "From: <" + config.sender.toUtf8() + ">\r\nTo: <" + DiagnosticReport::recipient().toUtf8() +
              ">\r\nSubject: Rapport Minato\r\nDate: " +
              QDateTime::currentDateTimeUtc().toString(Qt::RFC2822Date).toLatin1() + "\r\nMessage-ID: <" +
              QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1() +
              "@minato.local>\r\nMIME-Version: 1.0\r\nContent-Type: text/plain; "
              "charset=UTF-8\r\nContent-Transfer-Encoding: base64\r\n\r\n";
    const auto encoded = report.toBase64();
    for (qsizetype offset = 0; offset < encoded.size(); offset += 76)
        payload += encoded.mid(offset, 76) + "\r\n";
    payload += ".\r\n";
    state = State::Greeting;
    socket.setProtocol(QSsl::TlsV1_2OrLater);
    socket.setPeerVerifyMode(QSslSocket::VerifyPeer);
    socket.setPeerVerifyName(config.host);
    timeout.start(30000);
    if (config.security == Security::Tls)
        socket.connectToHostEncrypted(config.host, config.port);
    else
        socket.connectToHost(config.host, config.port);
}
void SmtpClient::command(const QByteArray &line, State next) {
    state = next;
    timeout.start(30000);
    socket.write(line + "\r\n");
}
void SmtpClient::readResponse() {
    if (!isActive())
        return;
    buffer += socket.readAll();
    if (buffer.size() + response.size() > 65536) {
        fail("Réponse SMTP trop volumineuse.");
        return;
    }
    while (isActive()) {
        const auto end = buffer.indexOf("\r\n");
        if (end < 0)
            return;
        const auto line = buffer.left(end);
        buffer.remove(0, end + 2);
        bool ok = false;
        const int code = line.left(3).toInt(&ok);
        if (!ok || code < 100 || code > 599 || line.size() < 4 || (line[3] != ' ' && line[3] != '-') ||
            (responseCode != 0 && code != responseCode)) {
            fail("Réponse SMTP invalide.");
            return;
        }
        responseCode = code;
        response += line.mid(4) + '\n';
        if (line[3] == '-')
            continue;
        const auto text = response;
        response.clear();
        responseCode = 0;
        handleResponse(code, text);
    }
}
void SmtpClient::authenticate(const QByteArray &capabilities) {
    if (!socket.isEncrypted()) {
        fail("Une connexion TLS est obligatoire avant l’authentification.");
        return;
    }
    QByteArray methods;
    for (const auto &line : capabilities.toUpper().split('\n'))
        if (line.startsWith("AUTH ") || line.startsWith("AUTH="))
            methods += line.mid(5) + ' ';
    if (methods.split(' ').contains("LOGIN"))
        command("AUTH LOGIN", State::AuthUser);
    else if (methods.split(' ').contains("PLAIN")) {
        QByteArray secret(1, '\0');
        secret += config.username.toUtf8();
        secret += '\0';
        secret += config.password.toUtf8();
        command("AUTH PLAIN " + secret.toBase64(), State::AuthResult);
        secret.fill('\0');
    } else
        fail("Le serveur ne propose pas AUTH LOGIN ou AUTH PLAIN. Un mot de passe d’application ou un autre "
             "serveur SMTP peut être nécessaire.");
}
void SmtpClient::handleResponse(int code, const QByteArray &text) {
    if (code >= 400) {
        fail("Le serveur SMTP a refusé la demande (code " + QString::number(code) +
             "). Vérifiez le compte, son mot de passe d’application et les droits d’envoi.");
        return;
    }
    switch (state) {
    case State::Greeting:
        if (code == 220) {
            command("EHLO minato.local", State::Hello);
            return;
        }
        break;
    case State::Hello:
        if (code == 250) {
            if (config.security == Security::StartTls) {
                bool supportsStartTls = false;
                for (const auto &line : text.toUpper().split('\n'))
                    if (line.trimmed() == "STARTTLS")
                        supportsStartTls = true;
                if (!supportsStartTls) {
                    fail("Le serveur ne propose pas STARTTLS. Envoi refusé.");
                    return;
                }
                command("STARTTLS", State::StartTls);
            } else
                authenticate(text);
            return;
        }
        break;
    case State::StartTls:
        if (code == 220) {
            socket.startClientEncryption();
            return;
        }
        break;
    case State::SecureHello:
        if (code == 250) {
            authenticate(text);
            return;
        }
        break;
    case State::AuthUser:
        if (code == 334) {
            command(config.username.toUtf8().toBase64(), State::AuthPassword);
            return;
        }
        break;
    case State::AuthPassword:
        if (code == 334) {
            command(config.password.toUtf8().toBase64(), State::AuthResult);
            return;
        }
        break;
    case State::AuthResult:
        if (code == 235) {
            config.password.fill(QChar('\0'));
            config.password.clear();
            command("MAIL FROM:<" + config.sender.toUtf8() + ">", State::Sender);
            return;
        }
        break;
    case State::Sender:
        if (code == 250) {
            command("RCPT TO:<" + DiagnosticReport::recipient().toUtf8() + ">", State::Recipient);
            return;
        }
        break;
    case State::Recipient:
        if (code == 250 || code == 251) {
            command("DATA", State::Data);
            return;
        }
        break;
    case State::Data:
        if (code == 354) {
            state = State::Accepted;
            timeout.start(60000);
            socket.write(payload);
            payload.fill('\0');
            payload.clear();
            return;
        }
        break;
    case State::Accepted:
        if (code == 250) {
            state = State::Idle;
            timeout.stop();
            clearSecrets();
            socket.write("QUIT\r\n");
            socket.disconnectFromHost();
            emit completed();
            return;
        }
        break;
    case State::Idle:
        return;
    }
    fail("Réponse inattendue du serveur SMTP. Le rapport n’a pas été confirmé.");
}
void SmtpClient::clearSecrets() {
    config.password.fill(QChar('\0'));
    config.password.clear();
    payload.fill('\0');
    payload.clear();
    buffer.clear();
    response.clear();
    responseCode = 0;
}
void SmtpClient::fail(const QString &message) {
    if (!isActive())
        return;
    state = State::Idle;
    timeout.stop();
    socket.abort();
    clearSecrets();
    emit failed(message);
}
void SmtpClient::cancel() {
    fail("Envoi interrompu. S’il avait déjà atteint le serveur, son retrait n’est pas garanti.");
}
