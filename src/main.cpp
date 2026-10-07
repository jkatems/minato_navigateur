#include "profiles/Profile.h"
#include "sync/DemoConfig.h"
#include "sync/SyncClient.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStandardPaths>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Minato");
    app.setWindowIcon(QIcon(":/icons/minato.svg"));
    app.setOrganizationName("Minato");
    app.setApplicationVersion("0.1.0");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"profile", "Profil isolé (lettres, chiffres, tirets)", "name", "Personnel"});
    parser.addOption({"private", "Session invitée sans persistance"});
    parser.addOption({"exam", "Examen local : envoi visible des visites/IP/MAC à Django sur 127.0.0.1:8000"});
    parser.addOption(
        {"remote-demo",
         "Démonstration distante consentie : visites/IP/MAC vers le serveur HTTPS préconfiguré"});
    parser.process(app);
    const auto name = parser.value("profile");
    if (!QRegularExpression("^[A-Za-z0-9_-]{1,40}$").match(name).hasMatch())
        return 2;
    const auto directory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/profiles/" + name;
    const bool guest = parser.isSet("private");
    if (!guest)
        QDir().mkpath(directory);
    QLockFile lock(directory + "/browser.lock");
    if (!guest && !lock.tryLock(100)) {
        QMessageBox::warning(nullptr, "Minato", "Ce profil est déjà ouvert ou son dossier est inaccessible.");
        return 1;
    }
    Profile profile(name, guest);
    if (parser.isSet("exam") && !guest)
        profile.sync->startExam();
    if (parser.isSet("remote-demo") && !guest) {
        const QUrl server(QString::fromUtf8(MinatoDemo::ServerUrl));
        if (server.host().contains("VOTRE-PROJET", Qt::CaseInsensitive) || server.scheme() != "https" ||
            !SyncClient::validServer(server)) {
            QMessageBox::warning(nullptr, "Serveur de démonstration",
                                 "Configurez l’URL HTTPS dans src/sync/DemoConfig.h puis recompilez.");
            return 2;
        }
        const auto answer = QMessageBox::question(
            nullptr, "Démonstration — partage de navigation",
            "Cette session transmettra les sites consultés, recherches, titres, IP locales et MAC exposées "
            "à " +
                server.toString() +
                ". Les rapports seront accessibles sans connexion à toute personne possédant cette URL. "
                "La démonstration dure au maximum une heure. Aucun cookie ni mot de passe de formulaire "
                "n’est envoyé. "
                "Utilisez uniquement des sites de démonstration. Autorisez-vous ce partage ?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return 0;
        profile.sync->startRemoteDemo(server, true);
    }
    MainWindow window(profile);
    window.show();
    return app.exec();
}
