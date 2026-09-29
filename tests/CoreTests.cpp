#include "browser/UrlResolver.h"
#include "database/Store.h"
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
class CoreTests : public QObject {
    Q_OBJECT
  private slots:
    void addresses_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("domain") << "example.com" << "https://example.com";
        QTest::newRow("http") << "http://localhost:8000/test" << "http://localhost:8000/test";
        QTest::newRow("ipv6") << "[::1]:8080" << "https://[::1]:8080";
        QTest::newRow("search") << "bonjour monde" << "https://duckduckgo.com/?q=bonjour%20monde";
        QTest::newRow("encoding") << "a & b" << "https://duckduckgo.com/?q=a%20%26%20b";
        QTest::newRow("internal") << "minato://network" << "minato://network";
        QTest::newRow("empty") << "  " << "minato://newtab";
        QTest::newRow("javascript") << "javascript:alert(1)" << "";
        QTest::newRow("file") << "file:///etc/passwd" << "";
        QTest::newRow("data") << "data:text/html,test" << "";
        QTest::newRow("ftp") << "ftp://example.com" << "";
        QTest::newRow("credentials") << "https://user:secret@example.com" << "";
    }
    void addresses() {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(Minato::resolveInput(input), QUrl(expected));
    }
    void database() {
        Store store(":memory:");
        QVERIFY(store.available());
        QVERIFY(store.visit("https://example.com", "Example"));
        QVERIFY(store.visit("https://example.com", "Example 2"));
        QCOMPARE(store.entries(false).first().visits, 2);
        QCOMPARE(store.entries(false).first().title, QString("Example 2"));
        QVERIFY(store.bookmark("https://example.com", "Quote ' and <html>"));
        QVERIFY(store.bookmark("https://example.com", "Updated"));
        QCOMPARE(store.entries(true).size(), 1);
        const auto id = store.entries(true).first().id;
        QVERIFY(store.editBookmark(id, "https://qt.io", "Qt"));
        QCOMPARE(store.entries(true, "Qt").size(), 1);
        QVERIFY(store.entries(true, "' OR 1=1 --").isEmpty());
        QVERIFY(store.remove(true, {id}));
        QVERIFY(store.entries(true).isEmpty());
        QVERIFY(store.clearHistory());
        QVERIFY(store.entries(false).isEmpty());
    }
    void persistenceAndIsolation() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            Store first(dir.filePath("first.sqlite"));
            QVERIFY(first.visit("https://example.com", "Persistent"));
        }
        {
            Store first(dir.filePath("first.sqlite"));
            QCOMPARE(first.entries(false).size(), 1);
            Store second(dir.filePath("second.sqlite"));
            QVERIFY(second.entries(false).isEmpty());
        }
    }
    void inaccessibleDatabase() {
        Store bad("/nonexistent-minato-test-dir/database.sqlite");
        QVERIFY(!bad.available());
        QVERIFY(!bad.error().isEmpty());
    }
};
QTEST_MAIN(CoreTests)
#include "CoreTests.moc"
