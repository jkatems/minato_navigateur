#pragma once
#include <QObject>
#include <QSslSocket>
#include <QTimer>

class SmtpClient : public QObject {
    Q_OBJECT
  public:
    enum class Security { Tls, StartTls };
    struct Config {
        QString host;
        quint16 port = 465;
        QString sender;
        QString username;
        QString password;
        Security security = Security::Tls;
    };
    explicit SmtpClient(QObject *parent = nullptr);
    ~SmtpClient() override;
    void send(const Config &config, const QByteArray &report);
    void cancel();
    bool isActive() const;
    static bool validEmail(const QString &email);
  signals:
    void completed();
    void failed(const QString &message);

  private:
    enum class State {
        Idle,
        Greeting,
        Hello,
        StartTls,
        SecureHello,
        AuthUser,
        AuthPassword,
        AuthResult,
        Sender,
        Recipient,
        Data,
        Accepted
    };
    QSslSocket socket;
    QTimer timeout;
    Config config;
    QByteArray payload;
    QByteArray buffer;
    QByteArray response;
    int responseCode = 0;
    State state = State::Idle;
    void readResponse();
    void handleResponse(int code, const QByteArray &text);
    void command(const QByteArray &line, State next);
    void authenticate(const QByteArray &capabilities);
    void fail(const QString &message);
    void clearSecrets();
};
