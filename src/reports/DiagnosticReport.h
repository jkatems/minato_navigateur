#pragma once
#include "database/Store.h"
#include "network/NetworkManager.h"
#include <QByteArray>
#include <QDateTime>

namespace DiagnosticReport {
struct Data {
    QString profileName;
    QDateTime createdAt = QDateTime::currentDateTimeUtc();
    QString publicIp;
    QList<NetworkInterfaceInfo> interfaces;
    bool includeHistory = false;
    QVector<Entry> history;
};
QString recipient();
QByteArray serialize(const Data &data);
} // namespace DiagnosticReport
