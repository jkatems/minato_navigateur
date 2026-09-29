#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
struct NetworkInterfaceInfo {
    QString name, type, state, ipv4, ipv6, mac;
};
class NetworkManager : public QObject {
    Q_OBJECT
  public:
    explicit NetworkManager(QObject *parent = nullptr);
    static QList<NetworkInterfaceInfo> interfaces();
    void fetchPublicIp();
    void setProvider(const QUrl &url);
  signals:
    void publicIpReady(const QString &message);

  private:
    QNetworkAccessManager manager;
    QUrl provider{QStringLiteral("https://api.ipify.org")};
    bool pending = false;
};
