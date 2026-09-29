#pragma once
#include <QUrl>
namespace Minato {
QUrl resolveInput(const QString &input, const QString &engine = "https://duckduckgo.com/?q=%1");
bool isWebUrl(const QUrl &url);
} // namespace Minato
