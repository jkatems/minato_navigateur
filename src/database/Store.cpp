#include "Store.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
Store::Store(const QString &path) : connection(QUuid::createUuid().toString()) {
    db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(path);
    if (!db.open()) {
        lastError = db.lastError().text();
        return;
    }
    execute("PRAGMA busy_timeout=3000");
    QSqlQuery version(db);
    version.exec("PRAGMA user_version");
    version.next();
    if (version.value(0).toInt() > 1) {
        lastError = "Version de base non prise en charge";
        db.close();
        return;
    }
    if (!db.transaction()) {
        lastError = db.lastError().text();
        db.close();
        return;
    }
    const bool ok =
        execute("CREATE TABLE IF NOT EXISTS history(id INTEGER PRIMARY KEY,url TEXT UNIQUE NOT NULL,title "
                "TEXT NOT NULL,time TEXT NOT NULL,visits INTEGER NOT NULL DEFAULT 1)") &&
        execute("CREATE TABLE IF NOT EXISTS bookmarks(id INTEGER PRIMARY KEY,url TEXT UNIQUE NOT NULL,title "
                "TEXT NOT NULL,time TEXT NOT NULL,visits INTEGER NOT NULL DEFAULT 0)") &&
        execute("PRAGMA user_version=1");
    if (!ok || !db.commit()) {
        db.rollback();
        db.close();
    }
}
Store::~Store() {
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
}
bool Store::available() const {
    return db.isOpen();
}
QString Store::error() const {
    return lastError;
}
bool Store::execute(const QString &sql, const QVariantList &values) const {
    QSqlQuery query(db);
    query.prepare(sql);
    for (const auto &value : values)
        query.addBindValue(value);
    if (!query.exec()) {
        lastError = query.lastError().text();
        return false;
    }
    return true;
}
bool Store::visit(const QString &url, const QString &title) {
    return execute(
        "INSERT INTO history(url,title,time) VALUES(?,?,strftime('%Y-%m-%dT%H:%M:%SZ','now')) ON "
        "CONFLICT(url) DO UPDATE SET title=excluded.title,time=excluded.time,visits=history.visits+1",
        {url, title});
}
bool Store::bookmark(const QString &url, const QString &title) {
    return execute("INSERT INTO bookmarks(url,title,time) VALUES(?,?,strftime('%Y-%m-%dT%H:%M:%SZ','now')) "
                   "ON CONFLICT(url) DO UPDATE SET title=excluded.title",
                   {url, title});
}
bool Store::editBookmark(qint64 id, const QString &url, const QString &title) {
    return execute("UPDATE bookmarks SET url=?,title=? WHERE id=?", {url, title, id});
}
QVector<Entry> Store::entries(bool bookmarks, const QString &search) const {
    QVector<Entry> result;
    QSqlQuery query(db);
    query.prepare(QString("SELECT id,url,title,time,visits FROM %1 WHERE url LIKE ? OR title LIKE ? ORDER BY "
                          "time DESC LIMIT 1000")
                      .arg(bookmarks ? "bookmarks" : "history"));
    query.addBindValue("%" + search + "%");
    query.addBindValue("%" + search + "%");
    if (!query.exec()) {
        lastError = query.lastError().text();
        return result;
    }
    while (query.next())
        result.push_back({query.value(0).toLongLong(), query.value(1).toString(), query.value(2).toString(),
                          query.value(3).toString(), query.value(4).toInt()});
    return result;
}
bool Store::remove(bool bookmarks, const QList<qint64> &ids) {
    if (!db.transaction())
        return false;
    for (auto id : ids)
        if (!execute(QString("DELETE FROM %1 WHERE id=?").arg(bookmarks ? "bookmarks" : "history"), {id})) {
            db.rollback();
            return false;
        }
    return db.commit();
}
bool Store::clearHistory() {
    return execute("DELETE FROM history");
}
