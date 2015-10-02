#include <QWidget>
#include <QCloseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPainter>
#include <QDebug>
#include "downloadmanager.h"
#include "genericfuncs.h"
#include "globalcontrol.h"
#include "ui_downloadmanager.h"

CDownloadManager::CDownloadManager(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CDownloadManager)
{
    ui->setupUi(this);

    firstResize = true;

    setWindowIcon(QIcon::fromTheme("folder-downloads"));

    model = new CDownloadsModel(this);
    ui->tableDownloads->setModel(model);
    ui->tableDownloads->verticalHeader()->hide();
    ui->tableDownloads->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableDownloads->setItemDelegateForColumn(2,new CDownloadBarDelegate(this,model));

    connect(ui->tableDownloads, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(contextMenu(QPoint)));

    connect(ui->buttonClearFinished, SIGNAL(clicked()), model, SLOT(cleanFinishedDownloads()));
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
    connect(item, SIGNAL(downloadProgress(qint64,qint64)),
            model, SLOT(downloadProgress(qint64,qint64)));
    connect(item, &QWebEngineDownloadItem::stateChanged,
            model, &CDownloadsModel::downloadStateChanged);

    item->setPath(fname);

    model->appendItem(CDownloadItem(item));

    item->accept();
}

void CDownloadManager::contextMenu(const QPoint &pos)
{
    QModelIndex idx = ui->tableDownloads->indexAt(pos);
    CDownloadItem item = model->getDownloadItem(idx);
    if (item.isEmpty()) return;

    QMenu *cm = new QMenu(this);
    QAction *acm;

    if (item.state==QWebEngineDownloadItem::DownloadInProgress) {
        acm = cm->addAction(tr("Abort"),model,SLOT(abortDownload()));
        acm->setData(idx.row());
        cm->addSeparator();
    }

    acm = cm->addAction(tr("Remove"),model,SLOT(cleanDownload()));
    acm->setData(idx.row());

    if (item.state==QWebEngineDownloadItem::DownloadCompleted ||
            item.state==QWebEngineDownloadItem::DownloadInProgress) {
        cm->addSeparator();
        acm = cm->addAction(tr("Open directory"),model,SLOT(openDirectory()));
        acm->setData(idx.row());
    }

    cm->setAttribute(Qt::WA_DeleteOnClose,true);
    cm->popup(ui->tableDownloads->mapToGlobal(pos));
}

void CDownloadManager::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void CDownloadManager::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);

    if (!firstResize) return;
    firstResize = false;

    ui->tableDownloads->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    ui->tableDownloads->setColumnWidth(1,350);
    ui->tableDownloads->setColumnWidth(2,200);
    ui->tableDownloads->horizontalHeader()->setStretchLastSection(true);
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

    CDownloadItem t = downloads.at(idx);
    if (role == Qt::DecorationRole) {
        if (index.column()==0) {
            switch (t.state) {
                case QWebEngineDownloadItem::DownloadCancelled:
                    return QIcon::fromTheme("task-reject").pixmap(QSize(16,16));
                case QWebEngineDownloadItem::DownloadCompleted:
                    return QIcon::fromTheme("task-complete").pixmap(QSize(16,16));
                case QWebEngineDownloadItem::DownloadInProgress:
                case QWebEngineDownloadItem::DownloadRequested:
                    return QIcon::fromTheme("arrow-right").pixmap(QSize(16,16));
                case QWebEngineDownloadItem::DownloadInterrupted:
                    return QIcon::fromTheme("task-attention").pixmap(QSize(16,16));
                default: return QVariant();
            }
            return QVariant();
        }
    } else if (role == Qt::DisplayRole) {
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
        return t.fileName;
    } else if (role == Qt::UserRole+1) {
        return 100*t.received/t.total;
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

CDownloadItem CDownloadsModel::getDownloadItem(const QModelIndex &index)
{
    if (!index.isValid()) return CDownloadItem();
    int idx = index.row();

    int maxl = downloads.count();
    if (idx<0 || idx>=maxl)
        return CDownloadItem();

    return downloads.at(idx);
}

void CDownloadsModel::deleteDownloadItem(const QModelIndex &index)
{
    if (!index.isValid()) return;
    int idx = index.row();

    int maxl = downloads.count();
    if (idx<0 || idx>=maxl) return;

    beginRemoveRows(QModelIndex(),idx,idx);
    downloads.removeAt(idx);
    endRemoveRows();
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

    downloads[idx].state = item->state();
    downloads[idx].ptr = NULL;

    emit dataChanged(index(idx,0),index(idx,3));

    item->deleteLater();

    if (downloads[idx].autoDelete)
        deleteDownloadItem(index(idx,0));
}

void CDownloadsModel::downloadStateChanged(QWebEngineDownloadItem::DownloadState state)
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

void CDownloadsModel::abortDownload()
{
    QAction *acm = qobject_cast<QAction *>(sender());
    if (acm==NULL) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    if (downloads.at(idx).state==QWebEngineDownloadItem::DownloadInProgress &&
            downloads.at(idx).ptr!=NULL) {
        downloads[idx].ptr->cancel();
    } else
        QMessageBox::warning(m_manager,tr("JPReader"),tr("Unable to stop this download. Incorrect state."));
}

void CDownloadsModel::cleanDownload()
{
    QAction *acm = qobject_cast<QAction *>(sender());
    if (acm==NULL) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    if (downloads.at(idx).state==QWebEngineDownloadItem::DownloadInProgress &&
            downloads.at(idx).ptr!=NULL) {
        downloads[idx].autoDelete = true;
        downloads[idx].ptr->cancel();
    } else
        deleteDownloadItem(index(idx,0));
}

void CDownloadsModel::cleanFinishedDownloads()
{
    int idx = 0;
    while (idx<downloads.count()) {
        if (downloads.at(idx).state!=QWebEngineDownloadItem::DownloadInProgress) {
            beginRemoveRows(QModelIndex(),idx,idx);
            downloads.removeAt(idx);
            endRemoveRows();
            idx = 0;
        } else
            idx++;
    }
}

void CDownloadsModel::openDirectory()
{
    QAction *acm = qobject_cast<QAction *>(sender());
    if (acm==NULL) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    QFileInfo fi(downloads.at(idx).fileName);

    if (!QProcess::startDetached("xdg-open", QStringList() << fi.path()))
        QMessageBox::critical(m_manager, tr("JPReader"), tr("Unable to start browser."));
}

CDownloadItem::CDownloadItem()
{
    id = 0;
    fileName.clear();
    state = QWebEngineDownloadItem::DownloadRequested;
    received = 0;
    total = 0;
    ptr = NULL;
    autoDelete = false;
    m_empty = true;
}

CDownloadItem::CDownloadItem(quint32 itemId)
{
    id = itemId;
    fileName.clear();
    state = QWebEngineDownloadItem::DownloadRequested;
    received = 0;
    total = 0;
    ptr = NULL;
    autoDelete = false;
    m_empty = false;
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
        autoDelete = false;
        m_empty = true;
    } else {
        id = item->id();
        fileName = item->path();
        state = item->state();
        received = item->receivedBytes();
        total = item->totalBytes();
        ptr = item;
        autoDelete = false;
        m_empty = false;
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
    autoDelete = other.autoDelete;
    m_empty = false;
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

bool CDownloadItem::isEmpty()
{
    return m_empty;
}

CDownloadBarDelegate::CDownloadBarDelegate(QObject *parent, CDownloadsModel *model)
    : QAbstractItemDelegate(parent)
{
    m_model = model;
}

void CDownloadBarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    if (index.column() == 2) {
            int progress = index.data(Qt::UserRole+1).toInt();

            QStyleOptionProgressBar progressBarOption;
            QRect r = option.rect;
            r.setHeight(r.height()-10); r.moveTop(5);
            progressBarOption.rect = r;
            progressBarOption.minimum = 0;
            progressBarOption.maximum = 100;
            progressBarOption.progress = progress;
            progressBarOption.text = QString::number(progress) + "%";
            progressBarOption.textVisible = true;

            QApplication::style()->drawControl(QStyle::CE_ProgressBar,
                                               &progressBarOption, painter);
        }
}

QSize CDownloadBarDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    return QSize(200,32);
}