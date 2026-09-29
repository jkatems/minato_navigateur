#include "BrowserTab.h"
#include "UrlResolver.h"
#include "profiles/Profile.h"
#include "ui/InternalPages.h"
#include <QLabel>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QWebEngineCertificateError>
#include <QWebEngineHistory>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineView>
BrowserPage::BrowserPage(QWebEngineProfile *profile, QObject *parent) : QWebEnginePage(profile, parent) {
    connect(this, &QWebEnginePage::certificateError, this,
            [](QWebEngineCertificateError error) { error.rejectCertificate(); });
}
bool BrowserPage::acceptNavigationRequest(const QUrl &url, NavigationType, bool) {
    // No local files, internal pages or privileged schemes reachable from remote content.
    return Minato::isWebUrl(url) || url == QUrl("about:blank");
}
BrowserTab::BrowserTab(Profile &p, QWidget *parent) : QWidget(parent), profile(p) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    error = new QLabel(this);
    error->setWordWrap(true);
    error->setTextFormat(Qt::PlainText);
    error->setObjectName("errorBanner");
    error->hide();
    layout->addWidget(error);
    stack = new QStackedLayout;
    layout->addLayout(stack);
    web = new QWebEngineView(this);
    web->setPage(new BrowserPage(profile.web.get(), web));
    stack->addWidget(web);
    connect(web, &QWebEngineView::urlChanged, this, [this](const QUrl &url) {
        if (stack->currentWidget() == web && url != QUrl("about:blank")) {
            current = url;
            if (pendingNavigation && navigationIndex >= 0)
                navigation[navigationIndex] = url;
            else {
                remember(url);
                pendingNavigation = true;
            }
            emit changed();
        }
    });
    connect(web, &QWebEngineView::titleChanged, this, [this](const QString &title) {
        if (!internal()) {
            pageTitle = title;
            emit changed();
        }
    });
    connect(web, &QWebEngineView::iconChanged, this, [this] { emit changed(); });
    connect(web, &QWebEngineView::loadProgress, this, &BrowserTab::progress);
    connect(web, &QWebEngineView::loadStarted, this, [this] {
        error->hide();
        emit changed();
    });
    connect(web, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (internal())
            return;
        pendingNavigation = false;
        if (ok && Minato::isWebUrl(web->url()) && profile.settings->value("history", true).toBool()) {
            if (!profile.store->visit(web->url().toString(), web->title())) {
                error->setText(
                    "L’historique ne peut pas être enregistré. Vérifiez l’accès au dossier du profil.");
                error->show();
            }
        } else if (!ok) {
            error->setText("Chargement interrompu ou impossible. Vérifiez votre connexion et l’adresse ; les "
                           "certificats non valides sont refusés.");
            error->show();
        }
        emit changed();
    });
    connect(web->page(), &QWebEnginePage::newWindowRequested, this, &BrowserTab::popup);
    connect(web->page(), &QWebEnginePage::windowCloseRequested, this, &BrowserTab::closeRequested);
}
void BrowserTab::navigate(const QUrl &url) {
    if (!url.isValid() || url.isEmpty()) {
        error->setText("Adresse invalide ou protocole non autorisé.");
        error->show();
        return;
    }
    if (url.scheme() == "minato") {
        web->stop();
        current = url;
        remember(url);
        pendingNavigation = false;
        if (internalPage) {
            stack->removeWidget(internalPage);
            internalPage->deleteLater();
        }
        internalPage = InternalPages::create(
            url.host(), profile, [this](const QUrl &target) { navigate(target); }, this);
        stack->addWidget(internalPage);
        stack->setCurrentWidget(internalPage);
        web->setUrl(QUrl("about:blank"));
        pageTitle = InternalPages::title(url.host());
        error->hide();
        emit progress(100);
        emit changed();
    } else if (Minato::isWebUrl(url)) {
        stack->setCurrentWidget(web);
        current = url;
        pageTitle = url.host();
        remember(url);
        pendingNavigation = true;
        error->hide();
        web->setUrl(url);
        emit changed();
    } else {
        error->setText("Seules les adresses HTTP, HTTPS et les pages internes Minato sont acceptées.");
        error->show();
    }
}
QUrl BrowserTab::url() const {
    return current;
}
QString BrowserTab::title() const {
    return pageTitle.isEmpty() ? "Nouvel onglet" : pageTitle;
}
QWebEngineView *BrowserTab::view() const {
    return web;
}
bool BrowserTab::internal() const {
    return stack->currentWidget() != web;
}
void BrowserTab::remember(const QUrl &url) {
    if (traversing) {
        traversing = false;
        return;
    }
    if (navigationIndex >= 0 && navigation[navigationIndex] == url)
        return;
    navigation.resize(navigationIndex + 1);
    navigation.push_back(url);
    navigationIndex = navigation.size() - 1;
}
bool BrowserTab::canBack() const {
    return navigationIndex > 0;
}
bool BrowserTab::canForward() const {
    return navigationIndex + 1 < navigation.size();
}
void BrowserTab::preparePopup() {
    stack->setCurrentWidget(web);
    current = QUrl("about:blank");
}
void BrowserTab::back() {
    if (!canBack())
        return;
    const auto target = navigation[--navigationIndex];
    if (!internal() && web->history()->canGoBack() && web->history()->backItem().url() == target) {
        pendingNavigation = true;
        web->back();
    } else {
        traversing = true;
        navigate(target);
    }
}
void BrowserTab::forward() {
    if (!canForward())
        return;
    const auto target = navigation[++navigationIndex];
    if (!internal() && web->history()->canGoForward() && web->history()->forwardItem().url() == target) {
        pendingNavigation = true;
        web->forward();
    } else {
        traversing = true;
        navigate(target);
    }
}
void BrowserTab::reload() {
    if (internal())
        navigate(current);
    else {
        pendingNavigation = true;
        web->reload();
    }
}
