#pragma once
#include <QMainWindow>
#include <QUrl>
class Profile;
class BrowserTab;
class QTabWidget;
class QLineEdit;
class QPushButton;
class QProgressBar;
class DownloadManager;
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(Profile &profile);
    ~MainWindow() override;
    BrowserTab *addTab(const QUrl &url = QUrl("minato://newtab"));

  protected:
    void closeEvent(QCloseEvent *event) override;

  private:
    Profile &profile;
    QTabWidget *tabs;
    QLineEdit *address;
    QPushButton *previous;
    QPushButton *next;
    QPushButton *reloadButton;
    QProgressBar *progress;
    DownloadManager *downloads;
    BrowserTab *active() const;
    void sync();
    void closeTab(int index);
    void saveSession();
};
