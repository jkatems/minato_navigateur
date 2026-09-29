#include "DownloadManager.h"
#include "profiles/Profile.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
DownloadManager::DownloadManager(Profile &profile, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Minato · Téléchargements");
    setObjectName("downloadsDialog");
    resize(660, 440);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Téléchargements de cette session"));
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *content = new QWidget;
    rows = new QVBoxLayout(content);
    rows->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll);
    connect(profile.web.get(), &QWebEngineProfile::downloadRequested, this,
            [this, &profile](QWebEngineDownloadRequest *request) {
                if (request->isSavePageDownload()) {
                    request->cancel();
                    return;
                }
                const auto folder =
                    profile.settings
                        ->value("downloads",
                                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
                        .toString();
                const auto path = QFileDialog::getSaveFileName(
                    this, "Enregistrer le téléchargement",
                    folder + "/" + QFileInfo(request->downloadFileName()).fileName());
                if (path.isEmpty()) {
                    request->cancel();
                    return;
                }
                const QFileInfo info(path);
                request->setDownloadDirectory(info.absolutePath());
                request->setDownloadFileName(info.fileName());
                auto *row = new QWidget;
                auto *layout = new QVBoxLayout(row);
                auto *name = new QLabel(info.fileName());
                name->setTextFormat(Qt::PlainText);
                layout->addWidget(name);
                auto *bar = new QProgressBar;
                layout->addWidget(bar);
                auto *state = new QLabel("En cours…");
                state->setTextFormat(Qt::PlainText);
                layout->addWidget(state);
                auto *actions = new QHBoxLayout;
                layout->addLayout(actions);
                auto *cancel = new QPushButton("Annuler");
                auto *open = new QPushButton("Ouvrir le dossier");
                open->setEnabled(false);
                actions->addWidget(cancel);
                actions->addWidget(open);
                rows->insertWidget(rows->count() - 1, row);
                connect(cancel, &QPushButton::clicked, request, &QWebEngineDownloadRequest::cancel);
                connect(open, &QPushButton::clicked, this,
                        [info] { QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath())); });
                auto update = [request, bar, state, cancel, open] {
                    const auto total = request->totalBytes();
                    bar->setRange(0, total > 0 ? 100 : 0);
                    if (total > 0)
                        bar->setValue(int(100.0 * request->receivedBytes() / total));
                    switch (request->state()) {
                    case QWebEngineDownloadRequest::DownloadCompleted:
                        state->setText("Terminé");
                        bar->setRange(0, 100);
                        bar->setValue(100);
                        break;
                    case QWebEngineDownloadRequest::DownloadCancelled:
                        state->setText("Annulé");
                        break;
                    case QWebEngineDownloadRequest::DownloadInterrupted:
                        state->setText("Échec : " + request->interruptReasonString());
                        break;
                    default:
                        state->setText(QString("%1 Ko reçus").arg(request->receivedBytes() / 1024));
                        break;
                    }
                    cancel->setEnabled(!request->isFinished());
                    open->setEnabled(request->state() == QWebEngineDownloadRequest::DownloadCompleted);
                };
                connect(request, &QWebEngineDownloadRequest::receivedBytesChanged, row, update);
                connect(request, &QWebEngineDownloadRequest::stateChanged, row, update);
                request->accept();
                show();
                update();
            });
}
