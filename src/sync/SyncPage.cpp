#include "SyncPage.h"
#include "SyncClient.h"
#include "profiles/Profile.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
QWidget *SyncPage::create(Profile &profile, QWidget *parent) {
    auto *root = new QWidget(parent);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    if (profile.sync->remoteDemoMode()) {
        auto *notice =
            new QLabel("Démonstration distante · les nouveaux sites, recherches, IP et MAC exposées "
                       "sont transmis à " +
                           profile.sync->server().toString() +
                           ". Les rapports sont accessibles sans connexion. Les données sont temporaires et "
                           "peuvent disparaître "
                           "ou différer entre instances du serveur. Aucun mot de passe de formulaire ni "
                           "cookie n’est envoyé.",
                       root);
        notice->setTextFormat(Qt::PlainText);
        notice->setWordWrap(true);
        layout->addWidget(notice);
        auto *state = new QLabel(profile.sync->status(), root);
        state->setWordWrap(true);
        layout->addWidget(state);
        auto *stop = new QPushButton("Arrêter la transmission", root);
        layout->addWidget(stop);
        QObject::connect(stop, &QPushButton::clicked, root, [&profile, stop] {
            profile.sync->stop();
            stop->setEnabled(false);
        });
        QObject::connect(profile.sync.get(), &SyncClient::statusChanged, root,
                         [&profile, state] { state->setText(profile.sync->status()); });
        return root;
    }
    if (profile.sync->examMode()) {
        auto *notice = new QLabel(
            "MODE EXAMEN LOCAL\n\nLes pages consultées, les recherches validées, "
            "les IP locales et les MAC exposées sont automatiquement envoyées à http://127.0.0.1:8000. "
            "Les rapports sont consultables sans connexion sur cette machine. Aucun mot de passe ni cookie "
            "n’est envoyé. Le mode privé est exclu. Fermez cette session Examen pour arrêter la "
            "transmission.",
            root);
        notice->setWordWrap(true);
        layout->addWidget(notice);
        auto *state = new QLabel(profile.sync->status(), root);
        state->setWordWrap(true);
        state->setTextFormat(Qt::PlainText);
        layout->addWidget(state);
        QObject::connect(profile.sync.get(), &SyncClient::statusChanged, root,
                         [&profile, state] { state->setText(profile.sync->status()); });
        return root;
    }
    auto *explanation = new QLabel(
        "Partage avec l’administrateur du parc : recherches validées, adresses saisies, pages chargées "
        "(URL, titre, date), nom du profil et interfaces réseau (IP locales, MAC exposées, type et état). "
        "Le serveur enregistre aussi l’IP de la connexion. Les administrateurs du serveur peuvent consulter "
        "ces données.\n\n"
        "Les sites visités n’ont pas accès à cette API. Aucun cookie, mot de passe de formulaire, contenu de "
        "page "
        "ou donnée de navigation privée n’est transmis. Les URL et les recherches peuvent toutefois contenir "
        "des informations sensibles. Le partage concerne les nouveaux événements, pas l’historique "
        "antérieur.",
        root);
    explanation->setWordWrap(true);
    explanation->setTextFormat(Qt::PlainText);
    layout->addWidget(explanation);
    auto *form = new QFormLayout;
    auto *host = new QLineEdit(profile.settings->value("sync/server").toString(), root);
    host->setObjectName("syncServer");
    host->setPlaceholderText("https://minato.votre-domaine.fr");
    auto *token = new QLineEdit(root);
    token->setObjectName("syncToken");
    token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText("Jeton de cet appareil, créé sur le serveur Django");
    form->addRow("Serveur du parc", host);
    form->addRow("Jeton d’écriture", token);
    layout->addLayout(form);
    auto *consent =
        new QCheckBox("J’autorise le partage de ces données avec les administrateurs de ce serveur.", root);
    consent->setObjectName("syncConsent");
    layout->addWidget(consent);
    auto *state = new QLabel(root);
    state->setObjectName("syncStatus");
    state->setWordWrap(true);
    state->setTextFormat(Qt::PlainText);
    layout->addWidget(state);
    auto *actions = new QHBoxLayout;
    auto *start = new QPushButton("Activer pour cette session", root);
    start->setObjectName("syncStart");
    auto *stop = new QPushButton("Arrêter le partage", root);
    stop->setObjectName("syncStop");
    actions->addWidget(start);
    actions->addWidget(stop);
    actions->addStretch();
    layout->addLayout(actions);
    const auto refresh = [&profile, state, start, stop, host, token, consent] {
        const bool active = profile.sync->active();
        state->setText(profile.guest ? "Mode invité/privé : partage interdit." : profile.sync->status());
        start->setEnabled(!profile.guest && !active);
        stop->setEnabled(active);
        host->setEnabled(!profile.guest && !active);
        token->setEnabled(!profile.guest && !active);
        consent->setEnabled(!profile.guest && !active);
    };
    QObject::connect(profile.sync.get(), &SyncClient::statusChanged, root, refresh);
    QObject::connect(start, &QPushButton::clicked, root, [&profile, root, host, token, consent] {
        const QUrl server(host->text().trimmed());
        if (!consent->isChecked()) {
            QMessageBox::information(root, "Partage",
                                     "Lisez les données partagées puis cochez votre autorisation.");
            return;
        }
        if (!SyncClient::validServer(server)) {
            QMessageBox::warning(root, "Serveur",
                                 "Indiquez l’origine HTTPS du serveur, sans chemin. HTTP est réservé à "
                                 "localhost/127.0.0.1 pour les tests locaux.");
            return;
        }
        if (QMessageBox::question(root, "Activer le partage",
                                  "Envoyer les nouvelles recherches, visites et informations réseau à " +
                                      server.toString() + " pour consultation par ses administrateurs ?",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        if (!profile.sync->start(server, token->text().trimmed(), true)) {
            QMessageBox::warning(root, "Jeton",
                                 "Jeton invalide : utilisez celui créé dans la page Appareils du serveur.");
            return;
        }
        profile.settings->setValue("sync/server", server.toString());
        profile.settings->sync();
        token->clear();
    });
    QObject::connect(stop, &QPushButton::clicked, root, [&profile, token, consent] {
        profile.sync->stop();
        token->clear();
        consent->setChecked(false);
    });
    auto *note = new QLabel(
        "Le jeton reste uniquement en mémoire : réactivez le partage à chaque lancement. "
        "La file d’envoi contient au plus 200 événements en mémoire, perdus à la fermeture ou à l’arrêt du "
        "partage. "
        "Arrêter n’efface pas les données déjà reçues : demandez leur suppression à l’administrateur. "
        "La conservation par défaut du serveur est de 30 jours si sa purge quotidienne est configurée.",
        root);
    note->setWordWrap(true);
    note->setObjectName("muted");
    layout->addWidget(note);
    refresh();
    return root;
}
