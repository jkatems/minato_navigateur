#include "reports/DiagnosticReport.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

class ReportTests : public QObject {
    Q_OBJECT
  private slots:
    void historyRequiresSelection() {
        DiagnosticReport::Data data;
        data.history.push_back(
            {1, "https://private.example/path", "Private title", "2026-01-01T10:00:00Z", 2});
        const auto report = DiagnosticReport::serialize(data);
        QVERIFY(!report.contains("private.example"));
        QVERIFY(!QJsonDocument::fromJson(report).object().contains("history"));
        data.includeHistory = true;
        const auto json = QJsonDocument::fromJson(DiagnosticReport::serialize(data)).object();
        QCOMPARE(json.value("history").toArray().size(), 1);
        QCOMPARE(json.value("history").toArray().first().toObject().value("visits").toInt(), 2);
    }
    void networkAndUnavailablePublicIp() {
        DiagnosticReport::Data data;
        data.profileName = "Personnel";
        data.interfaces.push_back(
            {"Interface \"test\"", "Wi-Fi", "Active", "192.0.2.10", "2001:db8::1", "Non exposée"});
        data.publicIp = "IP indisponible";
        auto json = QJsonDocument::fromJson(DiagnosticReport::serialize(data)).object();
        QVERIFY(json.value("public_ip").isNull());
        QCOMPARE(json.value("interfaces").toArray().size(), 1);
        QCOMPARE(json.value("interfaces").toArray().first().toObject().value("mac").toString(),
                 QString("Non exposée"));
        data.publicIp = "203.0.113.2";
        json = QJsonDocument::fromJson(DiagnosticReport::serialize(data)).object();
        QCOMPARE(json.value("public_ip").toString(), QString("203.0.113.2"));
        QVERIFY(!json.contains("cookies"));
        QVERIFY(!json.contains("passwords"));
    }
};
QTEST_GUILESS_MAIN(ReportTests)
#include "ReportTests.moc"
