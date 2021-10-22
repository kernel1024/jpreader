#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QDialog>
#include <QStyledItemDelegate>
#include <QAbstractTableModel>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QUuid>

#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
#include <QWebEngineDownloadItem>
using CDownloadState = QWebEngineDownloadItem::DownloadState;
using CDownloadPageFormat = QWebEngineDownloadItem::SavePageFormat;
using CDownloadInterruptReason = QWebEngineDownloadItem::DownloadInterruptReason;
#else
#include <QWebEngineDownloadRequest>
using CDownloadState = QWebEngineDownloadRequest::DownloadState;
using CDownloadPageFormat = QWebEngineDownloadRequest::SavePageFormat;
using CDownloadInterruptReason = QWebEngineDownloadRequest::DownloadInterruptReason;
#endif

namespace Ui {
class CDownloadManager;
}

class CDownloadsModel;
class CDownloadBarDelegate;
class CDownloadWriter;

class CDownloadItem
{
public:
    quint32 id { 0 };
    qint32 retries { 0 };
    bool aborted { false };
    QString pathName;
    QString mimeType;
    QString errorString;
    CDownloadState state { CDownloadState::DownloadRequested };
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    QPointer<QWebEngineDownloadItem> downloadItem;
#else
    QPointer<QWebEngineDownloadRequest> downloadItem;
#endif
    qint64 oldReceived { 0L };
    qint64 received { 0L };
    qint64 total { 0L };
    qint64 initialOffset { 0L };
    QPointer<QNetworkReply> reply;
    QPointer<CDownloadWriter> writer;
    QUuid auxId;
    QUrl url;

    bool autoDelete { false };

    CDownloadItem() = default;
    CDownloadItem(const CDownloadItem& other) = default;
    explicit CDownloadItem(quint32 itemId);
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    explicit CDownloadItem(QWebEngineDownloadItem* item);
#else
    explicit CDownloadItem(QWebEngineDownloadRequest* item);
#endif
    CDownloadItem(QNetworkReply* rpl, const QString& fname, const qint64 offset);
    explicit CDownloadItem(const QUuid &uuid);
    ~CDownloadItem() = default;
    CDownloadItem &operator=(const CDownloadItem& other) = default;
    bool operator==(const CDownloadItem &s) const;
    bool operator!=(const CDownloadItem &s) const;
    bool isEmpty() const;
    QString getFileName() const;
    QString getZipName() const;
    void reuseReply(QNetworkReply* rpl);
};

Q_DECLARE_METATYPE(CDownloadItem)

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

public Q_SLOTS:
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    void handleDownload(QWebEngineDownloadItem* item);
#else
    void handleDownload(QWebEngineDownloadRequest* item);
#endif
    void contextMenu(const QPoint& pos);
    void updateWriterStatus();

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

class CDownloadsModel : public QAbstractTableModel
{
    Q_OBJECT
private:
    CDownloadManager* m_manager;
    QVector<CDownloadItem> m_downloads;

    Q_DISABLE_COPY(CDownloadsModel)

    void updateProgressLabel();
    bool abortDownloadPriv(int row);
    void downloadFinishedPrev(QObject* source);

public:
    explicit CDownloadsModel(CDownloadManager* parent);
    ~CDownloadsModel() override;

    Qt::ItemFlags flags(const QModelIndex & index) const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    int rowCount( const QModelIndex & parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent) const override;

    CDownloadItem getDownloadItem(const QModelIndex & index);
    void deleteDownloadItem(const QModelIndex & index);
    void makeWriterJob(CDownloadItem &item) const;
    void createDownloadForNetworkRequest(const QNetworkRequest &request, const QString &fileName, qint64 offset,
                                         const QUuid reuseExistingDownloadItem = QUuid());

    void appendItem(const CDownloadItem& item);
    void auxDownloadProgress(qint64 bytesReceived, qint64 bytesTotal, QObject *source);

public Q_SLOTS:
    void downloadFinished();
    void downloadStateChanged(CDownloadState state);
    void requestRedirected(const QUrl &url);

    void abortDownload();
    void abortAll();
    void cleanDownload();
    void copyUrlToClipboard();
    void cleanFinishedDownloads();
    void cleanCompletedDownloads();
    void openDirectory();
    void openHere();
    void openXdg();

private Q_SLOTS:
    void writerError(const QString& message);
    void writerCompleted(bool success);

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
