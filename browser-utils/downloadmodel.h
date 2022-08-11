#ifndef DOWNLOADMODEL_H
#define DOWNLOADMODEL_H

#include <QAbstractTableModel>
#include <QPointer>
#include <QWebEngineProfile>
#include <QNetworkReply>
#include <QUuid>
#include <QQueue>
#include "downloadwriter.h"

namespace CDefaults {
const char zipSeparator = 0x00;
const int writerStatusTimerIntervalMS = 2000;
const int writerStatusLabelSize = 24;
const int downloadManagerColumnCount = 4;
const int retryRestartMS = 1000;
const auto replyAuxId = "replyAuxID";
const auto replyHeadFileName = "replyHeadFileName";
const auto replyHeadOffset = "replyHeadOffset";
}

class CDownloadManager;

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
    CDownloadItem(QNetworkReply* rpl, const QString& fname, qint64 offset);
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

class CDownloadTask
{
public:
    QNetworkRequest request;
    QString fileName;
    qint64 offset { 0L };
    CDownloadTask() = default;
    CDownloadTask(const QNetworkRequest& rq, const QString& fname, qint64 initialOffset);
    CDownloadTask(const CDownloadTask& other) = default;
    ~CDownloadTask() = default;
    CDownloadTask &operator=(const CDownloadTask& other) = default;
};

Q_DECLARE_METATYPE(CDownloadItem)


class CDownloadsModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_DISABLE_COPY(CDownloadsModel)
private:
    CDownloadManager* m_manager;
    QVector<CDownloadItem> m_downloads;
    QQueue<CDownloadTask> m_tasks;
    QMutex m_tasksMutex;

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
    bool createDownloadForNetworkRequest(const QNetworkRequest &request, const QString &fileName, qint64 offset,
                                         const QUuid &reuseExistingDownloadItem = QUuid());

    void appendItem(const CDownloadItem& item);
    void auxDownloadProgress(qint64 bytesReceived, qint64 bytesTotal, QObject *source);
    void checkPendingTasks();

public Q_SLOTS:
    void downloadFinished();
    void downloadStateChanged(CDownloadState state);
    void requestRedirected(const QUrl &url);

    void abortDownload();
    void abortActive();
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

#endif // DOWNLOADMODEL_H
