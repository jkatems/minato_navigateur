#include "InternalPages.h"
#include "browser/UrlResolver.h"
#include "network/NetworkManager.h"
#include "profiles/Profile.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWebEngineCookieStore>
namespace {
QLabel *label(const QString &text, const QString &name, QVBoxLayout *layout) {
    auto *widget = new QLabel(text);
    widget->setTextFormat(Qt::PlainText);
    widget->setWordWrap(true);
    widget->setObjectName(name);
    layout->addWidget(widget);
    return widget;
}
QPushButton *button(const QString &text, QBoxLayout *layout) {
    auto *b = new QPushButton(text);
    b->setCursor(Qt::PointingHandCursor);
    layout->addWidget(b);
    return b;
}
void failure(QWidget *parent) {
    QMessageBox::warning(parent, "Stockage indisponible",
                         "Impossible d’enregistrer cette modification. Vérifiez l’accès au dossier du profil "
                         "et l’espace disque.");
}
QTableWidget *table(const QStringList &headers, QVBoxLayout *layout) {
    auto *t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->verticalHeader()->hide();
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(260);
    layout->addWidget(t);
    return t;
}
void library(bool bookmarks, Profile &p, QVBoxLayout *layout, QWidget *root,
             InternalPages::Navigate navigate) {
    auto *search = new QLineEdit;
    search->setPlaceholderText("Rechercher par titre ou adresse…");
    layout->addWidget(search);
    auto *t = table({"Titre", "Adresse", "Dernière visite / ajout (UTC)", "Visites"}, layout);
    const auto refresh = [&p, t, search, bookmarks] {
        const auto entries = p.store->entries(bookmarks, search->text());
        t->setRowCount(entries.size());
        int row = 0;
        for (const auto &entry : entries) {
            const QStringList values{entry.title, entry.url, entry.time, QString::number(entry.visits)};
            for (int col = 0; col < values.size(); ++col)
                t->setItem(row, col, new QTableWidgetItem(values[col]));
            t->item(row, 0)->setData(Qt::UserRole, entry.id);
            ++row;
        }
    };
    QObject::connect(search, &QLineEdit::textChanged, root, [refresh] { refresh(); });
    QObject::connect(t, &QTableWidget::cellDoubleClicked, root,
                     [t, navigate](int row, int) { navigate(QUrl(t->item(row, 1)->text())); });
    auto *actions = new QHBoxLayout;
    layout->addLayout(actions);
    QObject::connect(button("Supprimer la sélection", actions), &QPushButton::clicked, root,
                     [&p, t, bookmarks, refresh, root] {
                         QList<qint64> ids;
                         for (const auto &index : t->selectionModel()->selectedRows())
                             ids << t->item(index.row(), 0)->data(Qt::UserRole).toLongLong();
                         if (!ids.isEmpty() && !p.store->remove(bookmarks, ids))
                             failure(root);
                         refresh();
                     });
    if (bookmarks) {
        QObject::connect(button("Modifier", actions), &QPushButton::clicked, root, [&p, t, refresh, root] {
            const int row = t->currentRow();
            if (row < 0)
                return;
            bool ok = false;
            const auto title = QInputDialog::getText(root, "Favori", "Titre", QLineEdit::Normal,
                                                     t->item(row, 0)->text(), &ok);
            if (!ok)
                return;
            const auto address = QInputDialog::getText(root, "Favori", "Adresse HTTPS ou HTTP",
                                                       QLineEdit::Normal, t->item(row, 1)->text(), &ok);
            if (!ok)
                return;
            const QUrl url(address);
            if (!Minato::isWebUrl(url)) {
                QMessageBox::warning(root, "Favori", "Adresse Web invalide.");
                return;
            }
            if (!p.store->editBookmark(t->item(row, 0)->data(Qt::UserRole).toLongLong(), url.toString(),
                                       title))
                failure(root);
            refresh();
        });
    } else {
        QObject::connect(
            button("Effacer tout l’historique", actions), &QPushButton::clicked, root, [&p, root, refresh] {
                if (QMessageBox::question(root, "Historique",
                                          "Effacer définitivement tout l’historique de ce profil ?") ==
                    QMessageBox::Yes) {
                    if (!p.store->clearHistory())
                        failure(root);
                    refresh();
                }
            });
    }
    actions->addStretch();
    refresh();
    label("Double-cliquez sur une ligne pour l’ouvrir. Les 1 000 résultats les plus récents sont affichés.",
          "muted", layout);
}
} // namespace
QString InternalPages::title(const QString &page) {
    static const QMap<QString, QString> titles{
        {"newtab", "Nouvel onglet"},      {"network", "Réseau"},         {"history", "Historique"},
        {"bookmarks", "Favoris"},         {"settings", "Paramètres"},    {"about", "À propos"},
        {"downloads", "Téléchargements"}, {"passwords", "Mots de passe"}};
    return titles.value(page, "Page introuvable");
}
QWidget *InternalPages::create(const QString &page, Profile &p, Navigate navigate, QWidget *parent) {
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *root = new QWidget;
    root->setObjectName("internalPage");
    scroll->setWidget(root);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(56, 38, 56, 38);
    layout->setSpacing(18);
    label("M I N A T O   /   " + (p.guest ? QString("INVITÉ") : p.name.toUpper()), "eyebrow", layout);
    label(page == "newtab" ? "Un nouvel horizon." : title(page), "heading", layout);
    if (page == "newtab") {
        label("L’espace pour explorer, apprendre et avancer.", "subtitle", layout);
        auto *search = new QLineEdit;
        search->setObjectName("homeSearch");
        search->setPlaceholderText("Rechercher sur le Web ou saisir une adresse");
        search->setClearButtonEnabled(true);
        layout->addWidget(search);
        QObject::connect(search, &QLineEdit::returnPressed, root, [search, &p, navigate] {
            navigate(Minato::resolveInput(
                search->text(), p.settings->value("search", "https://duckduckgo.com/?q=%1").toString()));
        });
        label("VOS ESCALES", "eyebrow", layout);
        auto *links = new QHBoxLayout;
        layout->addLayout(links);
        auto entries = p.store->entries(true);
        if (entries.isEmpty())
            entries = {{0, "https://www.wikipedia.org", "Wikipédia", {}, 0},
                       {0, "https://duckduckgo.com", "DuckDuckGo", {}, 0},
                       {0, "minato://network", "Mon réseau", {}, 0}};
        int count = 0;
        for (const auto &entry : entries) {
            if (++count > 5)
                break;
            auto *b = button(entry.title.left(28), links);
            b->setToolTip(entry.url);
            QObject::connect(b, &QPushButton::clicked, root,
                             [navigate, entry] { navigate(QUrl(entry.url)); });
        }
        links->addStretch();
        auto *shortcuts = new QHBoxLayout;
        layout->addLayout(shortcuts);
        for (const auto &target : QStringList{"history", "bookmarks", "settings"})
            QObject::connect(button(title(target), shortcuts), &QPushButton::clicked, root,
                             [navigate, target] { navigate(QUrl("minato://" + target)); });
        shortcuts->addStretch();
        label(p.guest ? "Session invitée · données supprimées à la fermeture."
                      : "Votre espace personnel · historique, favoris et sessions conservés dans ce profil.",
              "muted", layout);
    } else if (page == "history" || page == "bookmarks") {
        library(page == "bookmarks", p, layout, root, navigate);
    } else if (page == "network") {
        label(
            "Informations locales fournies par votre système. Elles ne sont pas communiquées aux sites Web.",
            "subtitle", layout);
        auto *t = table({"Interface", "Type", "État", "IPv4", "IPv6", "MAC exposée"}, layout);
        const auto refresh = [t] {
            const auto interfaces = NetworkManager::interfaces();
            t->setRowCount(interfaces.size());
            int row = 0;
            for (const auto &entry : interfaces) {
                const QStringList values{entry.name,
                                         entry.type,
                                         entry.state,
                                         entry.ipv4.isEmpty() ? "Indisponible" : entry.ipv4,
                                         entry.ipv6.isEmpty() ? "Indisponible" : entry.ipv6,
                                         entry.mac};
                for (int col = 0; col < values.size(); ++col) {
                    auto *item = new QTableWidgetItem(values[col]);
                    item->setToolTip(values[col]);
                    t->setItem(row, col, item);
                }
                ++row;
            }
            t->resizeRowsToContents();
        };
        refresh();
        auto *actions = new QHBoxLayout;
        layout->addLayout(actions);
        QObject::connect(button("Actualiser les interfaces", actions), &QPushButton::clicked, root, refresh);
        auto *publicButton = button("Consulter mon IP publique", actions);
        actions->addStretch();
        auto *result = label("IP publique non demandée. Fournisseur : api.ipify.org (HTTPS). Un clic "
                             "transmet votre IP à ce service.",
                             "muted", layout);
        auto *network = new NetworkManager(root);
        QObject::connect(publicButton, &QPushButton::clicked, root, [network, result, publicButton] {
            result->setText("Recherche de l’IP publique…");
            publicButton->setEnabled(false);
            network->fetchPublicIp();
        });
        QObject::connect(network, &NetworkManager::publicIpReady, root,
                         [result, publicButton](const QString &ip) {
                             result->setText(ip);
                             publicButton->setEnabled(true);
                         });
        label("La MAC peut être absente, masquée ou aléatoire. Le type de connexion est indiqué uniquement "
              "lorsqu’il est identifié par Qt ou le système.",
              "muted", layout);
    } else if (page == "settings") {
        label("Les préférences sont propres à ce profil.", "subtitle", layout);
        auto *form = new QFormLayout;
        form->setSpacing(16);
        layout->addLayout(form);
        auto *engine = new QComboBox;
        engine->addItem("DuckDuckGo", "https://duckduckgo.com/?q=%1");
        engine->addItem("Google", "https://www.google.com/search?q=%1");
        engine->addItem("Bing", "https://www.bing.com/search?q=%1");
        engine->setCurrentIndex(
            qMax(0, engine->findData(p.settings->value("search", "https://duckduckgo.com/?q=%1"))));
        form->addRow("Moteur de recherche", engine);
        auto *home = new QLineEdit(p.settings->value("home", "minato://newtab").toString());
        form->addRow("Page de démarrage", home);
        auto *restore = new QCheckBox("Restaurer les onglets au lancement");
        restore->setChecked(p.settings->value("restore", true).toBool());
        form->addRow("Session", restore);
        auto *history = new QCheckBox("Conserver l’historique");
        history->setChecked(p.settings->value("history", true).toBool());
        form->addRow("Confidentialité", history);
        auto *folder = new QLineEdit(
            p.settings->value("downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
                .toString());
        form->addRow("Dossier de téléchargement", folder);
        auto *appearance = new QComboBox;
        appearance->addItem("Nuit · menthe", "dark");
        appearance->addItem("Clair · océan", "light");
        appearance->setCurrentIndex(p.settings->value("theme", "dark") == "light" ? 1 : 0);
        form->addRow("Apparence (au redémarrage)", appearance);
        auto *actions = new QHBoxLayout;
        layout->addLayout(actions);
        auto *save = button("Enregistrer", actions);
        save->setObjectName("primary");
        save->setEnabled(!p.guest);
        QObject::connect(
            save, &QPushButton::clicked, root,
            [&p, engine, home, restore, history, folder, appearance, root] {
                const QUrl url(home->text().trimmed());
                if (!Minato::isWebUrl(url) &&
                    !(url.scheme() == "minato" && QStringList{"newtab", "settings", "history", "bookmarks",
                                                              "network", "about", "downloads", "passwords"}
                                                      .contains(url.host()))) {
                    QMessageBox::warning(root, "Paramètres", "Page de démarrage invalide.");
                    return;
                }
                p.settings->setValue("search", engine->currentData());
                p.settings->setValue("home", url.toString());
                p.settings->setValue("restore", restore->isChecked());
                p.settings->setValue("history", history->isChecked());
                p.settings->setValue("downloads", folder->text());
                p.settings->setValue("theme", appearance->currentData());
                p.settings->sync();
                if (p.settings->status() != QSettings::NoError)
                    failure(root);
                else
                    QMessageBox::information(root, "Minato", "Préférences enregistrées.");
            });
        actions->addStretch();
        auto *privacy = new QHBoxLayout;
        layout->addLayout(privacy);
        QObject::connect(button("Supprimer les cookies", privacy), &QPushButton::clicked, root, [&p, root] {
            if (QMessageBox::question(root, "Cookies",
                                      "Supprimer tous les cookies ? Les sites pourront vous déconnecter.") ==
                QMessageBox::Yes)
                p.web->cookieStore()->deleteAllCookies();
        });
        QObject::connect(button("Vider le cache", privacy), &QPushButton::clicked, root,
                         [&p] { p.web->clearHttpCache(); });
        for (const auto &target : QStringList{"history", "passwords", "network", "about"})
            QObject::connect(button(title(target), privacy), &QPushButton::clicked, root,
                             [navigate, target] { navigate(QUrl("minato://" + target)); });
        privacy->addStretch();
        label("Les cookies et le stockage Web sont persistants dans les profils normaux. Le mode invité "
              "utilise un profil WebEngine en mémoire. Effacer les cookies ne supprime pas le stockage local "
              "des sites.",
              "muted", layout);
        if (p.guest)
            label("Les préférences ne sont pas enregistrées en mode invité.", "muted", layout);
    } else if (page == "about") {
        label("Minato 0.1 · Votre fenêtre sur le Web.", "subtitle", layout);
        label("C++17 · Qt " + QString(qVersion()) +
                  " · Qt WebEngine · SQLite\nUn navigateur desktop pour Windows et Linux. Aucune télémétrie "
                  "ajoutée par Minato.",
              "body", layout);
        label("Version initiale : les DRM, certains codecs multimédias et certains fournisseurs de connexion "
              "peuvent dépendre de la distribution Qt et des règles du site.",
              "muted", layout);
    } else if (page == "passwords") {
        label("Le coffre-fort n’est pas activé dans cette V1.", "subtitle", layout);
        label("Minato ne capture et ne conserve aucun mot de passe. L’intégration Windows Credential Manager "
              "/ Linux Secret Service est prévue après validation de la V1.",
              "body", layout);
    } else if (page == "downloads") {
        label("Progression, annulation et accès aux fichiers de cette session.", "subtitle", layout);
        auto *actions = new QHBoxLayout;
        layout->addLayout(actions);
        QObject::connect(button("Afficher les téléchargements", actions), &QPushButton::clicked, root,
                         [root] {
                             if (auto *dialog = root->window()->findChild<QDialog *>("downloadsDialog"))
                                 dialog->show();
                         });
        actions->addStretch();
    } else {
        label("Cette page interne n’existe pas.", "subtitle", layout);
        auto *actions = new QHBoxLayout;
        layout->addLayout(actions);
        QObject::connect(button("Retour à l’accueil", actions), &QPushButton::clicked, root,
                         [navigate] { navigate(QUrl("minato://newtab")); });
        actions->addStretch();
    }
    layout->addStretch();
    return scroll;
}
