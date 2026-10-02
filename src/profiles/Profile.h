#pragma once
#include "database/Store.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QWebEngineProfile>
#include <memory>
class SyncClient;
class Profile {
  public:
    Profile(const QString &name, bool guest, const QString &dataRoot = {});
    ~Profile();
    QString name;
    bool guest;
    QString directory;
    std::unique_ptr<QTemporaryDir> temporary;
    std::unique_ptr<QSettings> settings;
    std::unique_ptr<Store> store;
    std::unique_ptr<QWebEngineProfile> web;
    std::unique_ptr<SyncClient> sync;
};
