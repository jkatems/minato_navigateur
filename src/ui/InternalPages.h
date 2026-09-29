#pragma once
#include <QString>
#include <QUrl>
#include <functional>
class QWidget;
class Profile;
namespace InternalPages {
using Navigate = std::function<void(const QUrl &)>;
QWidget *create(const QString &page, Profile &profile, Navigate navigate, QWidget *parent);
QString title(const QString &page);
} // namespace InternalPages
