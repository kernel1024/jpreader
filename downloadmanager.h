#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QDialog>
#include <QAbstractTableModel>
#include <QWebEngineDownloadItem>
#include <QAbstractItemDelegate>
#include <QNetworkReply>

namespace Ui {
class CDownloadManager;
}

class CDownloadsModel;
class CDownloadBarDelegate;

class CDownloadItem
{
    friend class CDownloadsModel;
private:
    bool m_empty;
    bool autoDelete;
    bool m_aux;
    QNetworkReply* m_rpl;
public:
    quint32 id;
    QString fileName, mimeType;
    QWebEngineDownloadItem::DownloadState state;
    qint64 received, total;
    QWebEngineDownloadItem* ptr;

    CDownloadItem();
    CDownloadItem(const CDownloadItem& other);
    CDownloadItem(quint32 itemId);
    CDownloadItem(QNetworkReply* rpl);
    CDownloadItem(QWebEngineDownloadItem* item);
    CDownloadItem(QNetworkReply* rpl, const QString& fname);
    CDownloadItem &operator=(const CDownloadItem& other);
    bool operator==(const CDownloadItem &s) const;
    bool operator!=(const CDownloadItem &s) const;
    bool isEmpty();
};

Q_DECLARE_METATYPE(CDownloadItem)

class CDownloadsModel;

class CDownloadManager : public QDialog
{
    Q_OBJECT

public:
    explicit CDownloadManager(QWidget *parent = 0);
    ~CDownloadManager();
    void handleAuxDownload(const QString &src, const QString &path, const QUrl& referer);
public slots:
    void handleDownload(QWebEngineDownloadItem* item);
    void contextMenu(const QPoint& pos);

private:
    CDownloadsModel *model;
    Ui::CDownloadManager *ui;
    bool firstResize;

protected:
    void closeEvent(QCloseEvent * event);
    void showEvent(QShowEvent * event);

};

class CDownloadsModel : public QAbstractTableModel
{
    Q_OBJECT
private:
    CDownloadManager* m_manager;
    QList<CDownloadItem> downloads;

public:
    explicit CDownloadsModel(CDownloadManager* parent);
    ~CDownloadsModel();

    Qt::ItemFlags flags(const QModelIndex & index) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount( const QModelIndex & parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent) const;

    CDownloadItem getDownloadItem(const QModelIndex & index);
    void deleteDownloadItem(const QModelIndex & index);

public slots:
    void appendItem(const CDownloadItem& item);

    void downloadFinished();
    void downloadFailed(QNetworkReply::NetworkError code);
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

public:
    CDownloadBarDelegate(QObject *parent, CDownloadsModel* model);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};


#endif // DOWNLOADMANAGER_H
