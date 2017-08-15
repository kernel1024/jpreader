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
#include "snviewer.h"
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

    connect(ui->tableDownloads, &QTableView::customContextMenuRequested,
            this, &CDownloadManager::contextMenu);

    connect(ui->buttonClearFinished, &QPushButton::clicked,
            model, &CDownloadsModel::cleanFinishedDownloads);
}

CDownloadManager::~CDownloadManager()
{
    delete ui;
}

void CDownloadManager::handleAuxDownload(const QString& src, const QString& path, const QUrl& referer)
{
    QUrl url = QUrl(src);
    if (!url.isValid() || url.isRelative()) return;

    QString fname = path;
    if (!fname.endsWith("/")) fname+="/";
    fname+=url.fileName();

    if (!isVisible())
        show();

    if (fname.isNull() || fname.isEmpty()) return;
    gSet->settings.savedAuxSaveDir = path;

    QNetworkRequest req(url);
    req.setRawHeader("referer",referer.toString().toUtf8());
    QNetworkReply* rpl = gSet->auxNetManager->get(req);

    connect(rpl, &QNetworkReply::finished,
            model, &CDownloadsModel::downloadFinished);
    connect(rpl, &QNetworkReply::downloadProgress,
            model, &CDownloadsModel::downloadProgress);
    connect(rpl, SIGNAL(error(QNetworkReply::NetworkError)),
            model, SLOT(downloadFailed(QNetworkReply::NetworkError)));

    model->appendItem(CDownloadItem(rpl, fname));
}


void CDownloadManager::handleDownload(QWebEngineDownloadItem *item)
{
    if (item==NULL) return;

    if (!isVisible())
        show();

    QFileInfo fi(item->path());

    QString fname = getSaveFileNameD(this,tr("Save file"),gSet->settings.savedAuxSaveDir,
                                     QString(),0,fi.fileName());

    if (fname.isNull() || fname.isEmpty()) {
        item->cancel();
        item->deleteLater();
        return;
    }
    gSet->settings.savedAuxSaveDir = QFileInfo(fname).absolutePath();

    connect(item, &QWebEngineDownloadItem::finished,
            model, &CDownloadsModel::downloadFinished);
    connect(item, &QWebEngineDownloadItem::downloadProgress,
            model, &CDownloadsModel::downloadProgress);
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
        acm = cm->addAction(tr("Open here"),model,SLOT(openHere()));
        acm->setData(idx.row());
        acm = cm->addAction(tr("Open with associated application"),model,SLOT(openXdg()));
        acm->setData(idx.row());
        acm = cm->addAction(tr("Open containing directory"),model,SLOT(openDirectory()));
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
            }
            return QVariant();
        }
    } else if (role == Qt::DisplayRole) {
        QFileInfo fi(t.fileName);
        switch (index.column()) {
            case 1: return fi.fileName();
            case 2:
                if (t.total==0) return QString("0%");
                return QString("%1%").arg(100*t.received/t.total);
            case 3:
                if (!t.errorString.isEmpty())
                    return t.errorString;
                return QString("%1 / %2")
                        .arg(formatBytes(t.received))
                        .arg(formatBytes(t.total));
            default: return QVariant();
        }
    } else if (role == Qt::ToolTipRole || role == Qt::StatusTipRole) {
        if (!t.errorString.isEmpty() && index.column()==0)
            return t.errorString;
        return t.fileName;
    } else if (role == Qt::UserRole+1) {
        if (t.total==0)
            return 0;
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

void CDownloadsModel::downloadFailed(QNetworkReply::NetworkError code)
{
    QNetworkReply* rpl = qobject_cast<QNetworkReply *>(sender());

    int idx = -1;
    QUrl url;

    if (rpl!=NULL) {
        idx = downloads.indexOf(CDownloadItem(rpl));
        url = rpl->url();
    }
    if (idx<0 || idx>=downloads.count()) return;

    downloads[idx].state = QWebEngineDownloadItem::DownloadInterrupted;
    downloads[idx].errorString = tr("Error %1: %2").arg(code).arg(rpl->errorString());
    downloads[idx].ptr = NULL;

    if (rpl!=NULL) {
        rpl->deleteLater();
        downloads[idx].m_rpl = NULL;
    }

    emit dataChanged(index(idx,0),index(idx,3));

    if (downloads[idx].autoDelete)
        deleteDownloadItem(index(idx,0));
}

void CDownloadsModel::downloadFinished()
{
    QWebEngineDownloadItem* item = qobject_cast<QWebEngineDownloadItem *>(sender());
    QNetworkReply* rpl = qobject_cast<QNetworkReply *>(sender());

    int idx = -1;

    if (item!=NULL)
        idx = downloads.indexOf(CDownloadItem(item->id()));
    else if (rpl!=NULL)
        idx = downloads.indexOf(CDownloadItem(rpl));
    if (idx<0 || idx>=downloads.count()) return;

    if (item!=NULL)
        downloads[idx].state = item->state();
    else
        downloads[idx].state = QWebEngineDownloadItem::DownloadCompleted;

    downloads[idx].ptr = NULL;

    if (rpl!=NULL) {
        QFile f(downloads[idx].fileName);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(rpl->readAll());
            f.close();
        } else
            downloads[idx].state = QWebEngineDownloadItem::DownloadCancelled;

        rpl->deleteLater();
        downloads[idx].m_rpl = NULL;
    }

    emit dataChanged(index(idx,0),index(idx,3));

    if (item!=NULL)
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
    if (item->interruptReason()!=QWebEngineDownloadItem::NoReason)
        downloads[idx].errorString = item->interruptReasonString();

    emit dataChanged(index(idx,0),index(idx,3));
}

void CDownloadsModel::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QWebEngineDownloadItem* item = qobject_cast<QWebEngineDownloadItem *>(sender());
    QNetworkReply* rpl = qobject_cast<QNetworkReply *>(sender());

    int idx = -1;

    if (item!=NULL)
        idx = downloads.indexOf(CDownloadItem(item->id()));
    else if (rpl!=NULL)
        idx = downloads.indexOf(CDownloadItem(rpl));
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

    bool ok = false;
    if (downloads.at(idx).state==QWebEngineDownloadItem::DownloadInProgress) {
        if (downloads.at(idx).ptr!=NULL) {
            downloads[idx].ptr->cancel();
            ok = true;
        } else if (downloads.at(idx).m_rpl!=NULL) {
            downloads[idx].m_rpl->abort();
            ok = true;
        }
    }

    if (!ok)
        QMessageBox::warning(m_manager,tr("JPReader"),tr("Unable to stop this download. Incorrect state."));
}

void CDownloadsModel::cleanDownload()
{
    QAction *acm = qobject_cast<QAction *>(sender());
    if (acm==NULL) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    bool ok = false;
    if (downloads.at(idx).state==QWebEngineDownloadItem::DownloadInProgress) {
        if (downloads.at(idx).ptr!=NULL) {
            downloads[idx].autoDelete = true;
            downloads[idx].ptr->cancel();
            ok = true;
        } else if (downloads.at(idx).m_rpl!=NULL) {
            downloads[idx].autoDelete = true;
            downloads[idx].m_rpl->abort();
            ok = true;
        }
    }
    if (!ok)
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

void CDownloadsModel::openHere()
{
    QAction *acm = qobject_cast<QAction *>(sender());
    if (acm==NULL) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    QStringList acceptedMime = { "text/plain", "text/html", "application/pdf",
                                 "image/jpeg", "image/png", "image/gif",
                                 "image/svg+xml", "image/webp" };
    QStringList acceptedExt = { "pdf", "htm", "html", "txt", "jpg", "jpeg", "jpe",
                                "png", "svg", "gif", "png", "webp" };

    QString fname = downloads.at(idx).fileName;
    QString mime = downloads.at(idx).mimeType;
    QFileInfo fi(fname);
    if ((acceptedMime.contains(mime,Qt::CaseInsensitive) ||
         acceptedExt.contains(fi.suffix(),Qt::CaseInsensitive))
            && (gSet->activeWindow!=NULL))
        new CSnippetViewer(gSet->activeWindow,QUrl::fromLocalFile(fname));
}

void CDownloadsModel::openXdg()
{
    QAction *acm = qobject_cast<QAction *>(sender());
    if (acm==NULL) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    if (!QProcess::startDetached("xdg-open", QStringList() << downloads.at(idx).fileName))
        QMessageBox::critical(m_manager, tr("JPReader"), tr("Unable to open application."));
}

CDownloadItem::CDownloadItem()
{
    id = 0;
    fileName.clear();
    mimeType.clear();
    errorString.clear();
    state = QWebEngineDownloadItem::DownloadRequested;
    received = 0;
    total = 0;
    ptr = NULL;
    autoDelete = false;
    m_empty = true;
    m_aux = false;
    m_rpl = NULL;
}

CDownloadItem::CDownloadItem(const CDownloadItem &other)
{
    id = other.id;
    fileName = other.fileName;
    mimeType = other.mimeType;
    errorString = other.errorString;
    state = other.state;
    received = other.received;
    total = other.total;
    ptr = other.ptr;
    autoDelete = other.autoDelete;
    m_aux = other.m_aux;
    m_empty = false;
    m_rpl = other.m_rpl;
}

CDownloadItem::CDownloadItem(quint32 itemId)
{
    id = itemId;
    fileName.clear();
    mimeType.clear();
    errorString.clear();
    state = QWebEngineDownloadItem::DownloadRequested;
    received = 0;
    total = 0;
    ptr = NULL;
    autoDelete = false;
    m_aux = false;
    m_empty = false;
    m_rpl = NULL;
}

CDownloadItem::CDownloadItem(QNetworkReply *rpl)
{
    id = 0;
    fileName.clear();
    mimeType.clear();
    errorString.clear();
    state = QWebEngineDownloadItem::DownloadRequested;
    received = 0;
    total = 0;
    ptr = NULL;
    autoDelete = false;
    m_aux = false;
    m_empty = false;
    m_rpl = rpl;
}

CDownloadItem::CDownloadItem(QWebEngineDownloadItem* item)
{
    if (item==NULL) {
        id = 0;
        fileName.clear();
        mimeType.clear();
        errorString.clear();
        state = QWebEngineDownloadItem::DownloadRequested;
        received = 0;
        total = 0;
        ptr = NULL;
        autoDelete = false;
        m_aux = false;
        m_empty = true;
        m_rpl = NULL;
    } else {
        id = item->id();
        fileName = item->path();
        mimeType = item->mimeType();
        errorString.clear();
        state = item->state();
        received = item->receivedBytes();
        total = item->totalBytes();
        ptr = item;
        autoDelete = false;
        m_aux = false;
        m_empty = false;
        m_rpl = NULL;
    }
}

CDownloadItem::CDownloadItem(QNetworkReply *rpl, const QString &fname)
{
    id = 0;
    fileName = fname;
    mimeType = QString("application/download");
    errorString.clear();
    state = QWebEngineDownloadItem::DownloadInProgress;
    received = 0;
    total = 0;
    ptr = NULL;
    autoDelete = false;
    m_aux = true;
    m_empty = false;
    m_rpl = rpl;
}

CDownloadItem &CDownloadItem::operator=(const CDownloadItem &other)
{
    id = other.id;
    fileName = other.fileName;
    mimeType = other.mimeType;
    errorString = other.errorString;
    state = other.state;
    received = other.received;
    total = other.total;
    ptr = other.ptr;
    autoDelete = other.autoDelete;
    m_empty = false;
    m_aux = other.m_aux;
    m_rpl = other.m_rpl;
    return *this;
}

bool CDownloadItem::operator==(const CDownloadItem &s) const
{
    if (id!=0 && s.id!=0)
        return (id==s.id);

    return m_rpl==s.m_rpl;
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
            r.setHeight(r.height()-10); r.moveTop(r.top()+5);
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
