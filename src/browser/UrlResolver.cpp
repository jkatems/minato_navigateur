#include "UrlResolver.h"
#include <QHostAddress>
#include <QRegularExpression>
namespace Minato {
bool isWebUrl(const QUrl &url) {
    return url.isValid() && !url.host().isEmpty() && (url.scheme() == "https" || url.scheme() == "http") &&
           url.userInfo().isEmpty();
}
QUrl resolveInput(const QString &input, const QString &engine) {
    const QString text = input.trimmed();
    if (text.isEmpty())
        return QUrl("minato://newtab");
    if (text.startsWith("minato://", Qt::CaseInsensitive))
        return QUrl(text);
    const QRegularExpression scheme("^[a-zA-Z][a-zA-Z0-9+.-]*://");
    if (scheme.match(text).hasMatch()) {
        const QUrl url(text);
        return isWebUrl(url) ? url : QUrl();
    }
    if (text.startsWith("javascript:", Qt::CaseInsensitive) ||
        text.startsWith("data:", Qt::CaseInsensitive) || text.startsWith("file:", Qt::CaseInsensitive))
        return {};
    const QUrl candidate("https://" + text);
    if (!text.contains(QRegularExpression("\\s")) &&
        (candidate.host().contains('.') || candidate.host() == "localhost" ||
         !QHostAddress(candidate.host()).isNull()))
        return isWebUrl(candidate) ? candidate : QUrl();
    return QUrl(engine.arg(QString::fromLatin1(QUrl::toPercentEncoding(text))));
}
} // namespace Minato
