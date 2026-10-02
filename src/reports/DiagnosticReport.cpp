#include "DiagnosticReport.h"
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QString DiagnosticReport::recipient() {
    return QStringLiteral("jkatemskatema@gmail.com");
}

QByteArray DiagnosticReport::serialize(const Data &data) {
    QJsonArray interfaces;
    for (const auto &iface : data.interfaces) {
        interfaces.append(QJsonObject{{"name", iface.name},
                                      {"type", iface.type},
                                      {"state", iface.state},
                                      {"ipv4", iface.ipv4},
                                      {"ipv6", iface.ipv6},
                                      {"mac", iface.mac}});
    }
    const QHostAddress publicAddress(data.publicIp);
    QJsonObject report{{"application", "Minato"},
                       {"schema_version", 1},
                       {"created_at_utc", data.createdAt.toUTC().toString(Qt::ISODate)},
                       {"profile", data.profileName},
                       {"public_ip", publicAddress.isNull() ? QJsonValue(QJsonValue::Null)
                                                            : QJsonValue(publicAddress.toString())},
                       {"interfaces", interfaces},
                       {"history_included", data.includeHistory}};
    if (data.includeHistory) {
        QJsonArray entries;
        for (const auto &entry : data.history) {
            entries.append(QJsonObject{{"url", entry.url},
                                       {"title", entry.title},
                                       {"last_visit_utc", entry.time},
                                       {"visits", entry.visits}});
        }
        report.insert("history", entries);
        report.insert("history_entry_limit", 1000);
    }
    return QJsonDocument(report).toJson(QJsonDocument::Indented);
}
