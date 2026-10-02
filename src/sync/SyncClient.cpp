#include "SyncClient.h"
#include "browser/UrlResolver.h"
#include "network/NetworkManager.h"
#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSslConfiguration>
#include <QUrlQuery>
#include <QUuid>

SyncClient::SyncClient(bool guest, QObject *parent) : QObject(parent), privateMode(guest) {
    retry.setSingleShot(true);
    connect(&retry, &QTimer::timeout, this, &SyncClient::flush);
}
SyncClient::~SyncClient() {
    enabled = false;
    if (reply) {
        reply->disconnect(this);
        reply->abort();
    }
    token.fill(QChar());
}
bool SyncClient::validServer(const QUrl &url) {
    // Explicit local development exception; no plaintext credentials across the LAN.
    const bool local = url.host() == "localhost" || QHostAddress(url.host()).isLoopback();
    return Minato::isWebUrl(url) && (url.scheme() == "https" || local) &&
           (url.path().isEmpty() || url.path() == "/") && !url.hasQuery() && !url.hasFragment();
}
bool SyncClient::start(const QUrl &url, const QString &credential, bool consent) {
    if (privateMode || !consent || !validServer(url) ||
        !QRegularExpression("^[A-Za-z0-9_-]{32,128}$").match(credential).hasMatch())
        return false;
    stop();
    exam = false;
    endpoint = url;
    endpoint.setPath("/api/v1/events/");
    token = credential;
    enabled = true;
    setStatus("Partage actif · " + endpoint.host() + " · en attente de navigation");
    return true;
}
bool SyncClient::startExam() {
    if (privateMode)
        return false;
    stop();
    exam = true;
    endpoint = QUrl("http://127.0.0.1:8000/api/v1/events/");
    enabled = true;
    setStatus("Examen local · transmission active vers 127.0.0.1:8000");
    return true;
}
bool SyncClient::examMode() const {
    return exam;
}
void SyncClient::stop() {
    enabled = false;
    retry.stop();
    if (reply) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
        reply = nullptr;
    }
    queue.clear();
    token.fill(QChar());
    token.clear();
    delay = 2000;
    setStatus(privateMode ? "Mode privé · aucun partage" : "Partage désactivé");
}
bool SyncClient::active() const {
    return enabled;
}
QString SyncClient::status() const {
    return message;
}
QUrl SyncClient::server() const {
    auto url = endpoint;
    url.setPath({});
    return url;
}
void SyncClient::setStatus(const QString &text) {
    message = text;
    emit statusChanged();
}
QUrl SyncClient::sanitized(const QUrl &url) {
    if (!Minato::isWebUrl(url))
        return {};
    QUrl result = url;
    result.setFragment({});
    QUrlQuery original(result), filtered;
    const QStringList sensitive{"password",      "passwd",        "pwd",     "token",     "access_token",
                                "refresh_token", "id_token",      "code",    "secret",    "api_key",
                                "apikey",        "authorization", "session", "sessionid", "auth"};
    for (const auto &item : original.queryItems())
        if (!sensitive.contains(item.first.toLower()))
            filtered.addQueryItem(item.first, item.second);
    result.setQuery(filtered);
    return result;
}
void SyncClient::recordInput(const QString &input, const QUrl &target, const QString &profile) {
    if (!enabled)
        return;
    // A local marker classifies input using the same resolver as navigation.
    const auto direct = Minato::resolveInput(input, QStringLiteral("minato-search://query?q=%1"));
    const bool search = direct.scheme() == "minato-search" && Minato::isWebUrl(target);
    record(search ? "search" : "address", target, {}, search ? input.trimmed() : QString(), profile);
}
void SyncClient::record(const QString &kind, const QUrl &url, const QString &title, const QString &query,
                        const QString &profile) {
    if (!enabled || privateMode || !QStringList{"search", "address", "visit"}.contains(kind))
        return;
    const auto clean = sanitized(url);
    if (clean.isEmpty() || clean.toString().size() > 8192)
        return;
    // Do not report visits to the management server itself.
    if (url.scheme() == endpoint.scheme() && url.host() == endpoint.host() &&
        url.port(url.scheme() == "https" ? 443 : 80) ==
            endpoint.port(endpoint.scheme() == "https" ? 443 : 80))
        return;
    if (queue.size() >= 200) {
        setStatus("Partage actif · file pleine : nouvel événement ignoré (200 en attente)");
        return;
    }
    QJsonArray interfaces;
    for (const auto &iface : NetworkManager::interfaces().mid(0, 64))
        interfaces.append(QJsonObject{{"name", iface.name.left(256)},
                                      {"type", iface.type.left(256)},
                                      {"state", iface.state.left(256)},
                                      {"ipv4", iface.ipv4.left(2048)},
                                      {"ipv6", iface.ipv6.left(2048)},
                                      {"mac", iface.mac.left(256)}});
    queue.enqueue(QJsonObject{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                              {"kind", kind},
                              {"occurred_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                              {"profile", profile.left(120)},
                              {"url", clean.toString()},
                              {"title", title.left(512)},
                              {"query", query.left(2048)},
                              {"interfaces", interfaces},
                              {"consent_version", 1}});
    if (!reply && !retry.isActive())
        retry.start(250);
}
void SyncClient::flush() {
    if (!enabled || reply || queue.isEmpty())
        return;
    // One event at a time bounds payload size even on systems with many interfaces.
    QJsonArray batch{queue.head()};
    const auto body = QJsonDocument(QJsonObject{{"events", batch}}).toJson(QJsonDocument::Compact);
    if (body.size() > 250000) {
        queue.dequeue();
        setStatus("Partage actif · événement trop volumineux ignoré");
        retry.start(250);
        return;
    }
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (exam)
        request.setRawHeader("X-Minato-Exam", "1");
    else
        request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setTransferTimeout(15000);
    auto ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    request.setSslConfiguration(ssl);
    reply = network.post(request, body);
    reply->setReadBufferSize(65536);
    const auto eventId = queue.head().value("id").toString();
    auto *currentReply = reply.data();
    connect(currentReply, &QNetworkReply::readyRead, this, [currentReply] {
        if (currentReply->bytesAvailable() >= 65536)
            currentReply->abort();
    });
    connect(currentReply, &QNetworkReply::finished, this, [this, currentReply, eventId] {
        const int code = currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto response = QJsonDocument::fromJson(currentReply->readAll()).object();
        const bool ok = currentReply->error() == QNetworkReply::NoError && code == 200 &&
                        response.value("accepted").toArray().contains(eventId);
        currentReply->deleteLater();
        reply = nullptr;
        if (!enabled)
            return;
        if (ok) {
            queue.dequeue();
            delay = 2000;
            setStatus("Partage actif · dernier envoi " + QTime::currentTime().toString("HH:mm:ss") + " · " +
                      QString::number(queue.size()) + " en attente");
            if (!queue.isEmpty())
                retry.start(300);
        } else if (code == 401 || code == 403 || (code >= 300 && code < 500 && code != 429)) {
            stop();
            setStatus("Partage arrêté · accès refusé ou configuration API invalide (HTTP " +
                      QString::number(code) + ")");
        } else {
            setStatus("Partage actif · serveur indisponible · " + QString::number(queue.size()) +
                      " en attente");
            delay = code == 429 ? 60000 : qMin(delay * 2, 60000);
            retry.start(delay);
        }
    });
}
