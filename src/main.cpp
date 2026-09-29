#include "profiles/Profile.h"
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
    MainWindow window(profile);
    window.show();
    return app.exec();
}
