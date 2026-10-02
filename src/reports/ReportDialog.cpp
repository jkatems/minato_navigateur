#include "ReportDialog.h"
#include "DiagnosticReport.h"
#include "SmtpClient.h"
#include "profiles/Profile.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ReportDialog::ReportDialog(Profile &p, const QString &ip, QWidget *parent)
    : QDialog(parent), profile(p), publicIp(ip) {
    setWindowTitle("Minato · Rapport par e-mail");
    resize(840, 740);
    auto *layout = new QVBoxLayout(this);
    auto *recipient = new QLabel("Destinataire : " + DiagnosticReport::recipient());
    recipient->setTextFormat(Qt::PlainText);
    layout->addWidget(recipient);
    auto *explanation = new QLabel(
        "Le rapport contient les interfaces réseau et les IP/MAC exposées par le système. L’IP publique est "
        "incluse si vous l’avez consultée sur la page réseau. Les URL de l’historique peuvent contenir des "
        "informations sensibles. Vérifiez l’aperçu avant l’envoi.");
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    includeHistory = new QCheckBox("Inclure l’historique de ce profil (1 000 dernières entrées)");
    includeHistory->setObjectName("includeReportHistory");
    layout->addWidget(includeHistory);
    preview = new QPlainTextEdit;
    preview->setReadOnly(true);
    preview->setObjectName("reportPreview");
    layout->addWidget(preview, 1);
    configuration = new QWidget;
    auto *form = new QFormLayout(configuration);
    layout->addWidget(configuration);
    host = new QLineEdit(profile.settings->value("smtp/host", "smtp.gmail.com").toString());
    host->setObjectName("smtpHost");
    form->addRow("Serveur SMTP", host);
    security = new QComboBox;
    security->addItem("TLS direct (SSL), généralement port 465");
    security->addItem("STARTTLS obligatoire, généralement port 587");
    security->setCurrentIndex(profile.settings->value("smtp/starttls", false).toBool() ? 1 : 0);
    form->addRow("Chiffrement", security);
    port = new QSpinBox;
    port->setRange(1, 65535);
    port->setValue(profile.settings->value("smtp/port", 465).toInt());
    form->addRow("Port", port);
    sender = new QLineEdit(profile.settings->value("smtp/sender").toString());
    form->addRow("Adresse de l’expéditeur", sender);
    username = new QLineEdit(profile.settings->value("smtp/username").toString());
    form->addRow("Identifiant SMTP", username);
    password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    password->setObjectName("smtpPassword");
    password->setPlaceholderText("Mot de passe d’application, non enregistré");
    form->addRow("Mot de passe SMTP", password);
    auto *note = new QLabel(
        "Les paramètres du serveur seront conservés dans ce profil, sauf en mode invité. Le mot de passe "
        "n’est pas enregistré. L’acceptation SMTP ne garantit pas la livraison en boîte de réception.");
    note->setWordWrap(true);
    layout->addWidget(note);
    status = new QLabel;
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    layout->addWidget(status);
    auto *actions = new QHBoxLayout;
    layout->addLayout(actions);
    auto *close = new QPushButton("Fermer / interrompre");
    actions->addWidget(close);
    actions->addStretch();
    sendButton = new QPushButton("Envoyer à " + DiagnosticReport::recipient());
    sendButton->setObjectName("sendReport");
    actions->addWidget(sendButton);
    client = new SmtpClient(this);
    connect(includeHistory, &QCheckBox::toggled, this, &ReportDialog::updatePreview);
    connect(security, &QComboBox::currentIndexChanged, this,
            [this](int index) { port->setValue(index == 1 ? 587 : 465); });
    connect(close, &QPushButton::clicked, this, &ReportDialog::reject);
    connect(sendButton, &QPushButton::clicked, this, &ReportDialog::send);
    connect(client, &SmtpClient::failed, this, [this](const QString &message) {
        setSending(false);
        status->setText(message);
        password->clear();
    });
    connect(client, &SmtpClient::completed, this, [this] {
        setSending(false);
        password->clear();
        sendButton->setEnabled(false);
        status->setText("Rapport accepté par le serveur de messagerie pour " + DiagnosticReport::recipient() +
                        ".");
    });
    updatePreview();
}
void ReportDialog::updatePreview() {
    DiagnosticReport::Data data;
    data.profileName = profile.guest ? "Invité" : profile.name;
    data.publicIp = publicIp;
    data.interfaces = NetworkManager::interfaces();
    data.includeHistory = includeHistory->isChecked();
    if (data.includeHistory)
        data.history = profile.store->entries(false);
    report = DiagnosticReport::serialize(data);
    preview->setPlainText(QString::fromUtf8(report));
    status->setText(data.includeHistory && !profile.store->available()
                        ? "L’historique est indisponible : la base locale n’est pas accessible."
                        : QString());
    sendButton->setEnabled(!data.includeHistory || profile.store->available());
}
void ReportDialog::setSending(bool sending) {
    configuration->setEnabled(!sending);
    includeHistory->setEnabled(!sending);
    sendButton->setEnabled(!sending);
}
void ReportDialog::send() {
    if (!SmtpClient::validEmail(sender->text().trimmed()) || host->text().trimmed().isEmpty() ||
        username->text().isEmpty() || password->text().isEmpty()) {
        status->setText("Renseignez le serveur, l’expéditeur, l’identifiant et le mot de passe SMTP.");
        return;
    }
    const auto message = "Envoyer le rapport affiché à " + DiagnosticReport::recipient() +
                         (includeHistory->isChecked() ? " avec l’historique de navigation de ce profil ?"
                                                      : " sans historique de navigation ?");
    if (QMessageBox::question(this, "Confirmer l’envoi", message, QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;
    SmtpClient::Config config;
    config.host = host->text().trimmed();
    config.port = quint16(port->value());
    config.sender = sender->text().trimmed();
    config.username = username->text();
    config.password = password->text();
    config.security =
        security->currentIndex() == 1 ? SmtpClient::Security::StartTls : SmtpClient::Security::Tls;
    if (!profile.guest) {
        profile.settings->setValue("smtp/host", config.host);
        profile.settings->setValue("smtp/port", config.port);
        profile.settings->setValue("smtp/sender", config.sender);
        profile.settings->setValue("smtp/username", config.username);
        profile.settings->setValue("smtp/starttls", security->currentIndex() == 1);
    }
    setSending(true);
    status->setText("Connexion sécurisée au serveur et envoi en cours…");
    client->send(config, report);
    password->clear();
}
void ReportDialog::reject() {
    if (client->isActive()) {
        if (QMessageBox::question(
                this, "Envoi en cours",
                "Interrompre la connexion ? Un message déjà reçu par le serveur ne peut pas être retiré.",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        client->cancel();
    }
    password->clear();
    QDialog::reject();
}
