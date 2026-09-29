#pragma once
#include <QDialog>
class Profile;
class QVBoxLayout;
class DownloadManager : public QDialog {
  public:
    DownloadManager(Profile &profile, QWidget *parent);

  private:
    QVBoxLayout *rows;
};
