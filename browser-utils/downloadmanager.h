#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QDialog>
#include <QStyledItemDelegate>
#include <QNetworkReply>
#include <QTimer>
#include <QWebEngineProfile>
#include "global/structures.h"

namespace Ui {
class CDownloadManager;
}

class CDownloadsModel;
class CZipWriter;

class CDownloadManager : public QDialog
{
    Q_OBJECT

public:
    CDownloadManager(QWidget *parent, CZipWriter *zipWriter);
    ~CDownloadManager() override;
    bool handleAuxDownload(const QString &src, const QString &suggestedFilename,
                           const QString &containerPath, const QUrl& referer, int index,
                           int maxIndex, bool isFanbox, bool isPatreon, bool &forceOverwrite);
    void setProgressLabel(const QString& text);
    qint64 receivedBytes() const;
    void addReceivedBytes(qint64 size);

    void multiFileDownload(const QVector<CUrlWithName> &urls, const QUrl &referer,
                           const QString &containerName, bool isFanbox, bool isPatreon);

public Q_SLOTS:
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    void handleDownload(QWebEngineDownloadItem* item);
#else
    void handleDownload(QWebEngineDownloadRequest* item);
#endif
    void contextMenu(const QPoint& pos);
    void updateWriterStatus();
    void novelReady(const QString &html, bool focus, bool translate, bool alternateTranslate);
    void mangaReady(const QVector<CUrlWithName> &urls, const QString &containerName, const QUrl &origin,
                    bool useViewer, bool focus, bool originalScale);
private Q_SLOTS:
    void headRequestFinished();
    void headRequestFailed(QNetworkReply::NetworkError error);
    void zipWriterError(const QString& message);

private:
    Ui::CDownloadManager *ui;
    CDownloadsModel *m_model;
    QTimer m_writerStatusTimer;
    qint64 m_receivedBytes { 0L };
    bool m_firstResize { true };
    int m_zipErrorCount { 0 };

    Q_DISABLE_COPY(CDownloadManager)

    bool computeFileName(QString &fileName, bool &isZipTarget, int index, int maxIndex, const QUrl &url,
                         const QString &containerPath, const QString &suggestedFilename) const;

protected:
    void closeEvent(QCloseEvent * event) override;
    void showEvent(QShowEvent * event) override;
    void hideEvent(QHideEvent * event) override;

};

class CDownloadBarDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY(CDownloadBarDelegate)
private:
    CDownloadsModel* m_model;
public:
    CDownloadBarDelegate(QObject *parent, CDownloadsModel* model);
    ~CDownloadBarDelegate() override = default;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // DOWNLOADMANAGER_H
