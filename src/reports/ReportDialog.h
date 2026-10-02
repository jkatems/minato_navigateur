#pragma once
#include <QDialog>
class Profile;
class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QComboBox;
class QSpinBox;
class SmtpClient;
class ReportDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ReportDialog(Profile &profile, const QString &publicIp = {}, QWidget *parent = nullptr);
    void reject() override;

  private:
    Profile &profile;
    QString publicIp;
    QByteArray report;
    QCheckBox *includeHistory;
    QPlainTextEdit *preview;
    QLineEdit *host;
    QSpinBox *port;
    QComboBox *security;
    QLineEdit *sender;
    QLineEdit *username;
    QLineEdit *password;
    QLabel *status;
    QPushButton *sendButton;
    SmtpClient *client;
    QWidget *configuration;
    void updatePreview();
    void send();
    void setSending(bool sending);
};
