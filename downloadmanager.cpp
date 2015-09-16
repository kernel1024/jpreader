#include <QWidget>
#include <QCloseEvent>
#include <QDebug>
#include "downloadmanager.h"
#include "genericfuncs.h"
#include "ui_downloadmanager.h"

CDownloadManager::CDownloadManager(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CDownloadManager)
{
    ui->setupUi(this);

    model = new CDownloadsModel(this);
    ui->tableDownloads->setModel(model);
    ui->tableDownloads->verticalHeader()->hide();
}

CDownloadManager::~CDownloadManager()
{
    delete ui;
}

void CDownloadManager::handleDownload(QWebEngineDownloadItem *item)
{
    if (item==NULL) return;

    if (!isVisible())
        show();

    QFileInfo fi(item->path());

    QString fname = getSaveFileNameD(this,tr("Save file"),QDir::homePath(),
                                     QString(),0,QFileDialog::DontUseNativeDialog,
                                     fi.fileName());

    if (fname.isNull() || fname.isEmpty()) {
        item->cancel();
        item->deleteLater();
        return;
    }

    connect(item, SIGNAL(finished()), model, SLOT(downloadFinished()));
    connect(item, SIGNAL(stateChanged(DownloadState)),
            model, SLOT(downloadStateChanged(DownloadState)));
    connect(item, SIGNAL(downloadProgress(qint64,qint64)),
            model, SLOT(downloadProgress(qint64,qint64)));

    item->setPath(fname);

    model->appendItem(CDownloadItem(item));

    item->accept();
}

void CDownloadManager::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void CDownloadManager::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);

    ui->tableDownloads->setColumnWidth(0,30);
    ui->tableDownloads->setColumnWidth(1,350);
    ui->tableDownloads->setColumnWidth(2,100);
    ui->tableDownloads->setColumnWidth(3,250);
}

CDownloadsModel::CDownloadsModel(CDownloadManager *parent)
    : QAbstractTableModel(parent)
{
    m_manager = parent;
    downloads.clear();
}

CDownloadsModel::~CDownloadsModel()
{
    downloads.clear();
}

Qt::ItemFlags CDownloadsModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

QVariant CDownloadsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();
    int idx = index.row();

    int maxl = downloads.count();
    if (idx<0 || idx>=maxl)
        return QVariant();

    if (role == Qt::DecorationRole) {
        if (index.column()==0) {
            switch (downloads.at(idx).state) {
                case QWebEngineDownloadItem::DownloadCancelled:
                    return QIcon::fromTheme("dialog-cancel").pixmap(QSize(16,16));
                case QWebEngineDownloadItem::DownloadCompleted:
                    return QIcon::fromTheme("dialog-ok-apply").pixmap(QSize(16,16));
                case QWebEngineDownloadItem::DownloadInProgress:
                case QWebEngineDownloadItem::DownloadRequested:
                    return QIcon::fromTheme("media-playback-start").pixmap(QSize(16,16));
                case QWebEngineDownloadItem::DownloadInterrupted:
                    return QIcon::fromTheme("media-playback-pause").pixmap(QSize(16,16));
                default: return QVariant();
            }
            return QVariant();
        }
    } else if (role == Qt::DisplayRole) {
        CDownloadItem t = downloads.at(idx);
        QFileInfo fi(t.fileName);
        switch (index.column()) {
            case 1: return fi.fileName();
            case 2: return QString("%1%").arg(100*t.received/t.total);
            case 3: return QString("%1 / %2")
                        .arg(formatBytes(t.received))
                        .arg(formatBytes(t.total));
            default: return QVariant();
        }
        return QVariant();
    } else if (role == Qt::ToolTipRole || role == Qt::StatusTipRole) {
        return downloads.at(idx).fileName;
    }
    return QVariant();

}

QVariant CDownloadsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(orientation);

    if (role == Qt::DisplayRole) {
        switch (section) {
            case 0: return tr(" ");
            case 1: return tr("Filename");
            case 2: return tr("Completion");
            case 3: return tr("Downloaded");
            default: return QVariant();
        }
        return QVariant();
    }
    return QVariant();

}

int CDownloadsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return downloads.count();
}

int CDownloadsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return 4;
}

void CDownloadsModel::appendItem(const CDownloadItem &item)
{
    int idx = downloads.count();
    beginInsertRows(QModelIndex(),idx,idx);
    downloads << item;
    endInsertRows();
}

void CDownloadsModel::downloadFinished()
{
    QWebEngineDownloadItem* item = qobject_cast<QWebEngineDownloadItem *>(sender());
    if (item==NULL) return;

    int idx = downloads.indexOf(CDownloadItem(item->id()));
    if (idx<0 || idx>=downloads.count()) return;

    downloads[idx].state = QWebEngineDownloadItem::DownloadCompleted;
    downloads[idx].received = downloads.at(idx).total;
    downloads[idx].ptr = NULL;

    emit dataChanged(index(idx,0),index(idx,3));

    item->deleteLater();
}

void CDownloadsModel::downloadStateChanged(DownloadState state)
{
    QWebEngineDownloadItem* item = qobject_cast<QWebEngineDownloadItem *>(sender());
    if (item==NULL) return;

    int idx = downloads.indexOf(CDownloadItem(item->id()));
    if (idx<0 || idx>=downloads.count()) return;

    downloads[idx].state = state;

    emit dataChanged(index(idx,0),index(idx,0));
}

void CDownloadsModel::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QWebEngineDownloadItem* item = qobject_cast<QWebEngineDownloadItem *>(sender());
    if (item==NULL) return;

    int idx = downloads.indexOf(CDownloadItem(item->id()));
    if (idx<0 || idx>=downloads.count()) return;

    downloads[idx].received = bytesReceived;
    downloads[idx].total = bytesTotal;

    emit dataChanged(index(idx,2),index(idx,3));
}

CDownloadItem::CDownloadItem()
{
    id = 0;
    fileName.clear();
    state = QWebEngineDownloadItem::DownloadRequested;
    received = 0;
    total = 0;
    ptr = NULL;
}

CDownloadItem::CDownloadItem(quint32 itemId)
{
    id = itemId;
    fileName.clear();
    state = QWebEngineDownloadItem::DownloadRequested;
    received = 0;
    total = 0;
    ptr = NULL;
}

CDownloadItem::CDownloadItem(QWebEngineDownloadItem* item)
{
    if (item==NULL) {
        id = 0;
        fileName.clear();
        state = QWebEngineDownloadItem::DownloadRequested;
        received = 0;
        total = 0;
        ptr = NULL;
    } else {
        id = item->id();
        fileName = item->path();
        state = item->state();
        received = item->receivedBytes();
        total = item->totalBytes();
        ptr = item;
    }
}

CDownloadItem &CDownloadItem::operator=(const CDownloadItem &other)
{
    id = other.id;
    fileName = other.fileName;
    state = other.state;
    received = other.received;
    total = other.total;
    ptr = other.ptr;
    return *this;
}

bool CDownloadItem::operator==(const CDownloadItem &s) const
{
    return (id==s.id);
}

bool CDownloadItem::operator!=(const CDownloadItem &s) const
{
    return !operator==(s);
}
