#include "Profile.h"
#include <QDir>
#include <QStandardPaths>
#include <QWebEngineSettings>
Profile::Profile(const QString &profileName, bool privateMode, const QString &dataRoot)
    : name(profileName), guest(privateMode) {
    const auto root =
        dataRoot.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : dataRoot;
    directory = root + "/profiles/" + name;
    if (guest) {
        temporary = std::make_unique<QTemporaryDir>();
        directory = temporary->path();
    } else
        QDir().mkpath(directory);
    settings = std::make_unique<QSettings>(directory + "/settings.ini", QSettings::IniFormat);
    store = std::make_unique<Store>(guest ? ":memory:" : directory + "/minato.sqlite");
    web = guest ? std::make_unique<QWebEngineProfile>() : std::make_unique<QWebEngineProfile>(name);
    if (!guest) {
        web->setPersistentStoragePath(directory + "/web");
        web->setCachePath(dataRoot.isEmpty()
                              ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + name
                              : dataRoot + "/cache/" + name);
        web->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        web->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    }
    web->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    web->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    web->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
}
Profile::~Profile() = default;
