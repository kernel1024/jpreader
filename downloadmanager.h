#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QDialog>
#include <QAbstractTableModel>
#include <QWebEngineDownloadItem>
#include <QAbstractItemDelegate>
#include <QNetworkReply>
#include <QPointer>

namespace Ui {
class CDownloadManager;
}

class CDownloadsModel;
class CDownloadBarDelegate;

class CDownloadItem
{
public:
    quint32 id { 0 };
    QString pathName;
    QString mimeType;
    QString errorString;
    QWebEngineDownloadItem::DownloadState state { QWebEngineDownloadItem::DownloadRequested };
    qint64 received { 0L };
    qint64 total { 0L };
    QPointer<QWebEngineDownloadItem> downloadItem;
    QPointer<QNetworkReply> reply;

    bool autoDelete { false };

    CDownloadItem() = default;
    CDownloadItem(const CDownloadItem& other) = default;
    explicit CDownloadItem(quint32 itemId);
    explicit CDownloadItem(QNetworkReply* rpl);
    explicit CDownloadItem(QWebEngineDownloadItem* item);
    CDownloadItem(QNetworkReply* rpl, const QString& fname);
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
    void handleAuxDownload(const QString &src, const QString &path, const QUrl& referer, int index,
                           int maxIndex, bool isFanbox);

public Q_SLOTS:
    void handleDownload(QWebEngineDownloadItem* item);
    void contextMenu(const QPoint& pos);

private:
    CDownloadsModel *model;
    Ui::CDownloadManager *ui;
    bool firstResize { true };

    Q_DISABLE_COPY(CDownloadManager)

protected:
    void closeEvent(QCloseEvent * event) override;
    void showEvent(QShowEvent * event) override;

};

class CDownloadsModel : public QAbstractTableModel
{
    Q_OBJECT
private:
    CDownloadManager* m_manager;
    QVector<CDownloadItem> downloads;

    Q_DISABLE_COPY(CDownloadsModel)

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

public Q_SLOTS:
    void appendItem(const CDownloadItem& item);

    void downloadFinished();
    void downloadStateChanged(QWebEngineDownloadItem::DownloadState state);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    void abortDownload();
    void cleanDownload();
    void cleanFinishedDownloads();
    void openDirectory();
    void openHere();
    void openXdg();

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
