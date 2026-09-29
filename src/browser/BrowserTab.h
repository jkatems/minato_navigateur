#pragma once
#include <QIcon>
#include <QUrl>
#include <QVector>
#include <QWebEnginePage>
#include <QWidget>
class Profile;
class QStackedLayout;
class QWebEngineView;
class QLabel;
class BrowserPage : public QWebEnginePage {
    Q_OBJECT
  public:
    BrowserPage(QWebEngineProfile *profile, QObject *parent);

  protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool mainFrame) override;
};
class BrowserTab : public QWidget {
    Q_OBJECT
  public:
    BrowserTab(Profile &profile, QWidget *parent = nullptr);
    void navigate(const QUrl &url);
    QUrl url() const;
    QString title() const;
    QWebEngineView *view() const;
    bool internal() const;
    void preparePopup();
    void back();
    void forward();
    void reload();
    bool canBack() const;
    bool canForward() const;
  signals:
    void changed();
    void progress(int value);
    void popup(QWebEngineNewWindowRequest &request);
    void closeRequested();

  private:
    Profile &profile;
    QStackedLayout *stack;
    QWebEngineView *web;
    QWidget *internalPage = nullptr;
    QLabel *error;
    QUrl current;
    QString pageTitle;
    QVector<QUrl> navigation;
    int navigationIndex = -1;
    bool traversing = false;
    bool pendingNavigation = false;
    void remember(const QUrl &url);
};
