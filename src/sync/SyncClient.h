#pragma once
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <QUrl>
class QNetworkReply;

// Tokens and the bounded retry queue live only in memory, never in WebEngine or QSettings.
class SyncClient : public QObject {
    Q_OBJECT
  public:
    explicit SyncClient(bool privateMode, QObject *parent = nullptr);
    ~SyncClient() override;
    bool start(const QUrl &server, const QString &token, bool consent);
    bool startExam();
    bool startRemoteDemo(const QUrl &server, bool consent);
    bool remoteDemoMode() const;
    bool examMode() const;
    void stop();
    bool active() const;
    QString status() const;
    QUrl server() const;
    void record(const QString &kind, const QUrl &url, const QString &title, const QString &query,
                const QString &profile);
    void recordInput(const QString &input, const QUrl &target, const QString &profile);
    static QUrl sanitized(const QUrl &url);
    static bool validServer(const QUrl &url);
  signals:
    void statusChanged();

  private:
    const bool privateMode;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> reply;
    QTimer retry;
    QQueue<QJsonObject> queue;
    QUrl endpoint;
    QString token;
    QString message = QStringLiteral("Partage désactivé");
    bool enabled = false;
    bool exam = false;
    bool remoteDemo = false;
    int delay = 2000;
    void flush();
    void setStatus(const QString &text);
};
