#include "NetworkManager.h"
#include "InterfaceType.h"
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QTimer>
NetworkManager::NetworkManager(QObject *parent) : QObject(parent), manager(this) {}
QList<NetworkInterfaceInfo> NetworkManager::interfaces() {
    QList<NetworkInterfaceInfo> result;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        QStringList v4, v6;
        for (const auto &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                v4 << entry.ip().toString();
            if (entry.ip().protocol() == QAbstractSocket::IPv6Protocol)
                v6 << entry.ip().toString();
        }
        const auto type = interfaceType(iface);
        auto mac = iface.hardwareAddress();
        if (mac.isEmpty() || mac == "00:00:00:00:00:00")
            mac = "Non exposée par le système";
        result.push_back(
            {iface.humanReadableName(), type,
             iface.flags().testFlag(QNetworkInterface::IsUp)
                 ? (iface.flags().testFlag(QNetworkInterface::IsRunning) ? "Active" : "Activée, sans liaison")
                 : "Inactive",
             v4.join("\n"), v6.join("\n"), mac});
    }
    return result;
}
void NetworkManager::setProvider(const QUrl &url) {
    if (url.scheme() == "https" && !url.host().isEmpty())
        provider = url;
}
void NetworkManager::fetchPublicIp() {
    if (pending)
        return;
    pending = true;
    QNetworkRequest request(provider);
    request.setTransferTimeout(8000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = manager.get(request);
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(10000);
    connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 256)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        pending = false;
        QHostAddress address;
        if (reply->error() == QNetworkReply::NoError)
            address.setAddress(QString::fromUtf8(reply->readAll()).trimmed());
        emit publicIpReady(
            reply->error() == QNetworkReply::NoError && !address.isNull()
                ? address.toString()
                : "IP publique indisponible : connexion absente, délai dépassé ou réponse invalide.");
        reply->deleteLater();
    });
}
