#pragma once
#include <QSqlDatabase>
#include <QVariant>
#include <QVector>
struct Entry {
    qint64 id;
    QString url;
    QString title;
    QString time;
    int visits;
};
class Store {
  public:
    explicit Store(const QString &path);
    ~Store();
    bool available() const;
    QString error() const;
    bool visit(const QString &url, const QString &title);
    bool bookmark(const QString &url, const QString &title);
    bool editBookmark(qint64 id, const QString &url, const QString &title);
    QVector<Entry> entries(bool bookmarks, const QString &search = {}) const;
    bool remove(bool bookmarks, const QList<qint64> &ids);
    bool clearHistory();

  private:
    QSqlDatabase db;
    QString connection;
    mutable QString lastError;
    bool execute(const QString &sql, const QVariantList &values = {}) const;
};
