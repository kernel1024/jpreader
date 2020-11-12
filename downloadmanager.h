#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QDialog>
#include <QAbstractTableModel>
#include <QWebEngineDownloadItem>
#include <QAbstractItemDelegate>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>

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
    QString pathName;
    QString mimeType;
    QString errorString;
    QWebEngineDownloadItem::DownloadState state { QWebEngineDownloadItem::DownloadRequested };
    qint64 oldReceived { 0L };
    qint64 received { 0L };
    qint64 total { 0L };
    qint64 initialOffset { 0L };
    QPointer<QWebEngineDownloadItem> downloadItem;
    QPointer<QNetworkReply> reply;
    QPointer<CDownloadWriter> writer;
    QUrl url;

    bool autoDelete { false };

    CDownloadItem() = default;
    CDownloadItem(const CDownloadItem& other) = default;
    explicit CDownloadItem(quint32 itemId);
    explicit CDownloadItem(QNetworkReply* rpl);
    explicit CDownloadItem(QWebEngineDownloadItem* item);
    explicit CDownloadItem(CDownloadWriter* w);
    CDownloadItem(QNetworkReply* rpl, const QString& fname, const qint64 offset);
    ~CDownloadItem() = default;
    CDownloadItem &operator=(const CDownloadItem& other) = default;
    bool operator==(const CDownloadItem &s) const;
    bool operator!=(const CDownloadItem &s) const;
    bool isEmpty() const;
    QString getFileName() const;
    QString getZipName() const;
};

Q_DECLARE_METATYPE(CDownloadItem)

class CDownloadsModel;

class CDownloadManager : public QDialog
{
    Q_OBJECT

public:
    explicit CDownloadManager(QWidget *parent = nullptr);
    ~CDownloadManager() override;
    bool handleAuxDownload(const QString &src, const QString &suggestedFilename,
                           const QString &containerPath, const QUrl& referer, int index,
                           int maxIndex, bool isFanbox, bool isPatreon, bool &forceOverwrite);
    void setProgressLabel(const QString& text);
    qint64 receivedBytes() const;
    void addReceivedBytes(qint64 size);

public Q_SLOTS:
    void handleDownload(QWebEngineDownloadItem* item);
    void contextMenu(const QPoint& pos);
    void updateWriterStatus();

private Q_SLOTS:
    void headRequestFinished();
    void headRequestFailed(QNetworkReply::NetworkError error);
    void requestRedirected(const QUrl &url);

private:
    Ui::CDownloadManager *ui;
    QHash<quintptr,QPair<QString,qint64> > m_headFileRequests;
    CDownloadsModel *m_model;
    QTimer m_writerStatusTimer;
    qint64 m_receivedBytes { 0L };
    bool m_firstResize { true };

    Q_DISABLE_COPY(CDownloadManager)

    bool computeFileName(QString &fileName, bool &isZipTarget, int index, int maxIndex, const QUrl &url,
                         const QString &containerPath, const QString &suggestedFilename) const;
    void createDownloadForNetworkRequest(const QNetworkRequest &request, const QString &fileName, qint64 offset);

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

public Q_SLOTS:
    void appendItem(const CDownloadItem& item);

    void downloadFinished();
    void downloadStateChanged(QWebEngineDownloadItem::DownloadState state);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

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

class CDownloadBarDelegate : public QAbstractItemDelegate {
    Q_OBJECT
private:
    CDownloadsModel* m_model;

    Q_DISABLE_COPY(CDownloadBarDelegate)

public:
    CDownloadBarDelegate(QObject *parent, CDownloadsModel* model);
    ~CDownloadBarDelegate() override = default;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};


#endif // DOWNLOADMANAGER_H
