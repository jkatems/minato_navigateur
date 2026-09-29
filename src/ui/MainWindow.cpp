#include "MainWindow.h"
#include "Theme.h"
#include "browser/BrowserTab.h"
#include "browser/UrlResolver.h"
#include "downloads/DownloadManager.h"
#include "profiles/Profile.h"
#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineView>
namespace {
QPushButton *tool(const QString &text, const QString &tip, QHBoxLayout *layout) {
    auto *b = new QPushButton(text);
    b->setObjectName("toolButton");
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(38, 38);
    layout->addWidget(b);
    return b;
}

} // namespace
MainWindow::MainWindow(Profile &p) : profile(p) {
    applyTheme(profile.settings->value("theme", "dark") == "light");
    setWindowTitle("Minato");
    resize(1280, 840);
    setMinimumSize(860, 580);
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setCentralWidget(central);
    auto *navigation = new QHBoxLayout;
    navigation->setContentsMargins(14, 10, 14, 10);
    navigation->setSpacing(8);
    auto *brand = new QLabel("◈  MINATO");
    brand->setObjectName("eyebrow");
    brand->setMinimumWidth(112);
    navigation->addWidget(brand);
    previous = tool("←", "Précédent · Alt+Gauche", navigation);
    next = tool("→", "Suivant · Alt+Droite", navigation);
    reloadButton = tool("↻", "Actualiser / arrêter · Ctrl+R / Échap", navigation);
    auto *home = tool("⌂", "Accueil · Alt+Début", navigation);
    address = new QLineEdit;
    address->setObjectName("addressBar");
    address->setAccessibleName("Adresse ou recherche");
    address->setPlaceholderText("Rechercher ou saisir une adresse");
    address->setClearButtonEnabled(true);
    navigation->addWidget(address, 1);
    auto *bookmark = tool("☆", "Ajouter aux favoris · Ctrl+D", navigation);
    auto *download = tool("↓", "Téléchargements", navigation);
    auto *menuButton = tool("☰", "Menu Minato", navigation);
    layout->addLayout(navigation);
    progress = new QProgressBar;
    progress->setFixedHeight(3);
    progress->setTextVisible(false);
    progress->setRange(0, 100);
    progress->setValue(0);
    layout->addWidget(progress);
    tabs = new QTabWidget;
    tabs->setDocumentMode(true);
    tabs->setTabsClosable(true);
    tabs->setMovable(true);
    tabs->tabBar()->setExpanding(false);
    tabs->tabBar()->setDrawBase(false);
    tabs->tabBar()->setElideMode(Qt::ElideRight);
    layout->addWidget(tabs, 1);
    auto *plus = new QPushButton("+");
    plus->setToolTip("Nouvel onglet · Ctrl+T");
    plus->setFixedSize(40, 36);
    tabs->setCornerWidget(plus, Qt::TopRightCorner);
    downloads = new DownloadManager(profile, this);
    connect(download, &QPushButton::clicked, downloads, &QDialog::show);
    connect(plus, &QPushButton::clicked, this, [this] { addTab(); });
    connect(tabs, &QTabWidget::currentChanged, this, [this] {
        if (active())
            address->setText(active()->url().toString());
        sync();
    });
    connect(address, &QLineEdit::editingFinished, this, &MainWindow::sync);
    connect(tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(address, &QLineEdit::returnPressed, this, [this] {
        if (active()) {
            const auto target = Minato::resolveInput(
                address->text(),
                profile.settings->value("search", "https://duckduckgo.com/?q=%1").toString());
            active()->setFocus();
            active()->navigate(target);
        }
    });
    connect(previous, &QPushButton::clicked, this, [this] {
        if (active())
            active()->back();
    });
    connect(next, &QPushButton::clicked, this, [this] {
        if (active())
            active()->forward();
    });
    connect(reloadButton, &QPushButton::clicked, this, [this] {
        if (active()) {
            if (!active()->internal() && active()->view()->page()->isLoading())
                active()->view()->stop();
            else
                active()->reload();
        }
    });
    connect(home, &QPushButton::clicked, this, [this] {
        if (active())
            active()->navigate(QUrl(profile.settings->value("home", "minato://newtab").toString()));
    });
    connect(bookmark, &QPushButton::clicked, this, [this] {
        if (!active() || !Minato::isWebUrl(active()->url()))
            return;
        if (profile.store->bookmark(active()->url().toString(), active()->title()))
            statusBar()->showMessage("Favori enregistré", 3000);
        else
            QMessageBox::warning(this, "Favoris",
                                 "Impossible d’enregistrer le favori : stockage indisponible.");
    });
    auto *menu = new QMenu(this);
    menuButton->setMenu(menu);
    menu->addAction("Nouvel onglet", this, [this] { addTab(); });
    menu->addAction("Dupliquer l’onglet", this, [this] {
        if (active())
            addTab(active()->url());
    });
    menu->addSeparator();
    for (const auto &page :
         QStringList{"bookmarks", "history", "settings", "network", "about", "passwords"}) {
        const QMap<QString, QString> names{{"bookmarks", "Favoris"},   {"history", "Historique"},
                                           {"settings", "Paramètres"}, {"network", "Réseau"},
                                           {"about", "À propos"},      {"passwords", "Mots de passe"}};
        menu->addAction(names.value(page), this, [this, page] { addTab(QUrl("minato://" + page)); });
    }
    menu->addSeparator();
    menu->addAction("Quitter", this, &QWidget::close);
    const auto shortcut = [this](const QKeySequence &key, auto callback) {
        auto *s = new QShortcut(key, this);
        connect(s, &QShortcut::activated, this, callback);
    };
    shortcut(QKeySequence("Ctrl+T"), [this] { addTab(); });
    shortcut(QKeySequence("Ctrl+W"), [this] { closeTab(tabs->currentIndex()); });
    shortcut(QKeySequence("Ctrl+L"), [this] {
        address->setFocus();
        address->selectAll();
    });
    shortcut(QKeySequence("Ctrl+Tab"),
             [this] { tabs->setCurrentIndex((tabs->currentIndex() + 1) % tabs->count()); });
    shortcut(QKeySequence("Ctrl+Shift+Tab"),
             [this] { tabs->setCurrentIndex((tabs->currentIndex() + tabs->count() - 1) % tabs->count()); });
    shortcut(QKeySequence("Ctrl+R"), [this] {
        if (active())
            active()->reload();
    });
    shortcut(QKeySequence("F5"), [this] {
        if (active())
            active()->reload();
    });
    shortcut(QKeySequence("Escape"), [this] {
        if (active())
            active()->view()->stop();
    });
    shortcut(QKeySequence("Alt+Left"), [this] {
        if (active())
            active()->back();
    });
    shortcut(QKeySequence("Alt+Right"), [this] {
        if (active())
            active()->forward();
    });
    shortcut(QKeySequence("Alt+Home"), [home] { home->click(); });
    shortcut(QKeySequence("Ctrl+D"), [bookmark] { bookmark->click(); });
    shortcut(QKeySequence("Ctrl+H"), [this] { addTab(QUrl("minato://history")); });
    shortcut(QKeySequence("Ctrl+J"), [this] { downloads->show(); });
    shortcut(QKeySequence("Ctrl++"), [this] {
        if (active())
            active()->view()->setZoomFactor(qMin(3.0, active()->view()->zoomFactor() + 0.1));
    });
    shortcut(QKeySequence("Ctrl+-"), [this] {
        if (active())
            active()->view()->setZoomFactor(qMax(0.3, active()->view()->zoomFactor() - 0.1));
    });
    shortcut(QKeySequence("Ctrl+0"), [this] {
        if (active())
            active()->view()->setZoomFactor(1);
    });
    QStringList session;
    if (!profile.guest && profile.settings->value("restore", true).toBool())
        session = profile.settings->value("tabs").toStringList();
    for (const auto &url : session.mid(0, 30)) {
        const QUrl target(url);
        if (Minato::isWebUrl(target) || target.scheme() == "minato")
            addTab(target);
    }
    if (tabs->count() == 0)
        addTab(QUrl(profile.settings->value("home", "minato://newtab").toString()));
    if (!profile.guest)
        tabs->setCurrentIndex(qBound(0, profile.settings->value("activeTab", 0).toInt(), tabs->count() - 1));
    statusBar()->showMessage(profile.guest ? "Invité · session éphémère" : "Profil : " + profile.name);
    if (!profile.store->available())
        statusBar()->showMessage("Base locale indisponible : historique et favoris désactivés.");
}
MainWindow::~MainWindow() {
    delete takeCentralWidget();
}
BrowserTab *MainWindow::active() const {
    return qobject_cast<BrowserTab *>(tabs->currentWidget());
}
BrowserTab *MainWindow::addTab(const QUrl &url) {
    auto *tab = new BrowserTab(profile, tabs);
    const int index = tabs->addTab(tab, "Nouvel onglet");
    connect(tab, &BrowserTab::changed, this, [this, tab] {
        const int index = tabs->indexOf(tab);
        if (index < 0)
            return;
        tabs->setTabText(index, tab->title().left(32));
        tabs->setTabToolTip(index, tab->title());
        tabs->setTabIcon(index, tab->internal() ? QIcon() : tab->view()->icon());
        if (tab == active())
            sync();
    });
    connect(tab, &BrowserTab::progress, this, [this, tab](int value) {
        tab->setProperty("progress", value);
        if (tab == active())
            progress->setValue(value == 100 ? 0 : value);
    });
    connect(tab, &BrowserTab::popup, this, [this](QWebEngineNewWindowRequest &request) {
        if (!request.isUserInitiated()) {
            statusBar()->showMessage("Fenêtre automatique bloquée", 4000);
            return;
        }
        auto *target = addTab();
        target->preparePopup();
        request.openIn(target->view()->page());
    });
    connect(tab, &BrowserTab::closeRequested, this, [this, tab] { closeTab(tabs->indexOf(tab)); });
    tabs->setCurrentIndex(index);
    tab->navigate(url);
    address->setText(tab->url().toString());
    return tab;
}
void MainWindow::sync() {
    auto *tab = active();
    if (!tab)
        return;
    if (!address->hasFocus())
        address->setText(tab->url().toString());
    previous->setEnabled(tab->canBack());
    next->setEnabled(tab->canForward());
    reloadButton->setText(!tab->internal() && tab->view()->page()->isLoading() ? "×" : "↻");
    setWindowTitle(tab->title() + " — Minato");
    const int value = tab->property("progress").toInt();
    progress->setValue(value == 100 ? 0 : value);
}
void MainWindow::closeTab(int index) {
    if (index < 0)
        return;
    auto *widget = tabs->widget(index);
    tabs->removeTab(index);
    delete widget;
    if (tabs->count() == 0)
        addTab();
}
void MainWindow::saveSession() {
    if (profile.guest)
        return;
    QStringList urls;
    for (int i = 0; i < tabs->count(); ++i)
        urls << qobject_cast<BrowserTab *>(tabs->widget(i))->url().toString();
    profile.settings->setValue("tabs", urls);
    profile.settings->setValue("activeTab", tabs->currentIndex());
    profile.settings->sync();
}
void MainWindow::closeEvent(QCloseEvent *event) {
    saveSession();
    QMainWindow::closeEvent(event);
}
