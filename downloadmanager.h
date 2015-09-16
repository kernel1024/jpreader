#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QDialog>
#include <QAbstractTableModel>
#include <QWebEngineDownloadItem>

namespace Ui {
class CDownloadManager;
}

class CDownloadItem
{
public:
    quint32 id;
    QString fileName;
    QWebEngineDownloadItem::DownloadState state;
    qint64 received, total;
    QWebEngineDownloadItem* ptr;

    CDownloadItem();
    CDownloadItem(quint32 itemId);
    CDownloadItem(QWebEngineDownloadItem* item);
    CDownloadItem &operator=(const CDownloadItem& other);
    bool operator==(const CDownloadItem &s) const;
    bool operator!=(const CDownloadItem &s) const;
};

Q_DECLARE_METATYPE(CDownloadItem)

class CDownloadsModel;

class CDownloadManager : public QDialog
{
    Q_OBJECT

public:
    explicit CDownloadManager(QWidget *parent = 0);
    ~CDownloadManager();

public slots:
    void handleDownload(QWebEngineDownloadItem* item);

private:
    CDownloadsModel *model;
    Ui::CDownloadManager *ui;

protected:
    void closeEvent(QCloseEvent * event);
    void showEvent(QShowEvent * event);

};

typedef QWebEngineDownloadItem::DownloadState DownloadState;

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

public slots:
    void appendItem(const CDownloadItem& item);

    void downloadFinished();
    void downloadStateChanged(DownloadState state);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

};

#endif // DOWNLOADMANAGER_H
