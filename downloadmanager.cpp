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

    setWindowIcon(QIcon::fromTheme(QStringLiteral("folder-downloads")));

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

void CDownloadManager::handleAuxDownload(const QString& src, const QString& path, const QUrl& referer,
                                         int index, int maxIndex)
{
    const int indexBase = 10;

    QUrl url = QUrl(src);
    if (!url.isValid() || url.isRelative()) return;

    QString fname = path;
    if (!fname.endsWith('/')) fname.append('/');
    if (index>=0) {
        fname.append(QStringLiteral("%1_")
                     .arg(index,CGenericFuncs::numDigits(maxIndex),indexBase,QLatin1Char('0')));
    }
    fname.append(url.fileName());

    if (!isVisible())
        show();

    if (fname.isNull() || fname.isEmpty()) return;
    gSet->setSavedAuxSaveDir(path);

    QNetworkRequest req(url);
    req.setRawHeader("referer",referer.toString().toUtf8());
    QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

    connect(rpl, &QNetworkReply::finished,
            model, &CDownloadsModel::downloadFinished);
    connect(rpl, &QNetworkReply::downloadProgress,
            model, &CDownloadsModel::downloadProgress);
    connect(rpl, qOverload<QNetworkReply::NetworkError>(&QNetworkReply::error),
            model, &CDownloadsModel::downloadFailed);

    model->appendItem(CDownloadItem(rpl, fname));
}


void CDownloadManager::handleDownload(QWebEngineDownloadItem *item)
{
    if (item==nullptr) return;

    if (!isVisible())
        show();

    if (item->savePageFormat()==QWebEngineDownloadItem::UnknownSaveFormat) {
        // Async save request from user, not full html page
        QFileInfo fi(item->path());

        QString fname = CGenericFuncs::getSaveFileNameD(this,tr("Save file"),gSet->settings()->savedAuxSaveDir,
                                                        QStringList(),nullptr,fi.fileName());

        if (fname.isNull() || fname.isEmpty()) {
            item->cancel();
            item->deleteLater();
            return;
        }
        gSet->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

        item->setPath(fname);
    }

    connect(item, &QWebEngineDownloadItem::finished,
            model, &CDownloadsModel::downloadFinished);
    connect(item, &QWebEngineDownloadItem::downloadProgress,
            model, &CDownloadsModel::downloadProgress);
    connect(item, &QWebEngineDownloadItem::stateChanged,
            model, &CDownloadsModel::downloadStateChanged);

    model->appendItem(CDownloadItem(item));

    item->accept();
}

void CDownloadManager::contextMenu(const QPoint &pos)
{
    QModelIndex idx = ui->tableDownloads->indexAt(pos);
    CDownloadItem item = model->getDownloadItem(idx);
    if (item.isEmpty()) return;

    QMenu cm;
    QAction *acm;

    if (item.state==QWebEngineDownloadItem::DownloadInProgress) {
        acm = cm.addAction(tr("Abort"),model,&CDownloadsModel::abortDownload);
        acm->setData(idx.row());
        cm.addSeparator();
    }

    acm = cm.addAction(tr("Remove"),model,&CDownloadsModel::cleanDownload);
    acm->setData(idx.row());

    if (item.state==QWebEngineDownloadItem::DownloadCompleted ||
            item.state==QWebEngineDownloadItem::DownloadInProgress) {
        cm.addSeparator();
        acm = cm.addAction(tr("Open here"),model,&CDownloadsModel::openHere);
        acm->setData(idx.row());
        acm = cm.addAction(tr("Open with associated application"),model,&CDownloadsModel::openXdg);
        acm->setData(idx.row());
        acm = cm.addAction(tr("Open containing directory"),model,&CDownloadsModel::openDirectory);
        acm->setData(idx.row());
    }

    cm.exec(ui->tableDownloads->mapToGlobal(pos));
}

void CDownloadManager::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void CDownloadManager::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

    if (!firstResize) return;
    firstResize = false;

    const int filenameColumnWidth = 350;
    const int completionColumnWidth = 200;

    ui->tableDownloads->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    ui->tableDownloads->setColumnWidth(1,filenameColumnWidth);
    ui->tableDownloads->setColumnWidth(2,completionColumnWidth);
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
    Q_UNUSED(index)

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

QVariant CDownloadsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();
    int idx = index.row();

    int maxl = downloads.count();
    if (idx<0 || idx>=maxl)
        return QVariant();

    const QSize iconSize(16,16);

    CDownloadItem t = downloads.at(idx);
    if (role == Qt::DecorationRole) {
        if (index.column()==0) {
            switch (t.state) {
                case QWebEngineDownloadItem::DownloadCancelled:
                    return QIcon::fromTheme(QStringLiteral("task-reject")).pixmap(iconSize);
                case QWebEngineDownloadItem::DownloadCompleted:
                    return QIcon::fromTheme(QStringLiteral("task-complete")).pixmap(iconSize);
                case QWebEngineDownloadItem::DownloadInProgress:
                case QWebEngineDownloadItem::DownloadRequested:
                    return QIcon::fromTheme(QStringLiteral("arrow-right")).pixmap(iconSize);
                case QWebEngineDownloadItem::DownloadInterrupted:
                    return QIcon::fromTheme(QStringLiteral("task-attention")).pixmap(iconSize);
            }
            return QVariant();
        }
    } else if (role == Qt::DisplayRole) {
        QFileInfo fi(t.fileName);
        switch (index.column()) {
            case 1: return fi.fileName();
            case 2:
                if (t.total==0) return QStringLiteral("0%");
                return QStringLiteral("%1%").arg(100*t.received/t.total);
            case 3:
                if (!t.errorString.isEmpty())
                    return t.errorString;
                return QStringLiteral("%1 / %2")
                        .arg(CGenericFuncs::formatFileSize(t.received),
                             CGenericFuncs::formatFileSize(t.total));
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
    Q_UNUSED(orientation)

    if (role == Qt::DisplayRole) {
        switch (section) {
            case 0: return QStringLiteral(" ");
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
    Q_UNUSED(parent)

    return downloads.count();
}

int CDownloadsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

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
    auto rpl = qobject_cast<QNetworkReply *>(sender());

    int idx = -1;
    QUrl url;

    if (rpl) {
        idx = downloads.indexOf(CDownloadItem(rpl));
        url = rpl->url();
    }
    if (idx<0 || idx>=downloads.count()) return;

    downloads[idx].state = QWebEngineDownloadItem::DownloadInterrupted;
    downloads[idx].errorString = tr("Error %1: %2").arg(code).arg(rpl->errorString());
    downloads[idx].downloadItem = nullptr;

    if (rpl) {
        rpl->deleteLater();
        downloads[idx].reply = nullptr;
    }

    Q_EMIT dataChanged(index(idx,0),index(idx,3));

    if (downloads.at(idx).autoDelete)
        deleteDownloadItem(index(idx,0));
}

void CDownloadsModel::downloadFinished()
{
    auto item = qobject_cast<QWebEngineDownloadItem *>(sender());
    auto rpl = qobject_cast<QNetworkReply *>(sender());

    int idx = -1;

    if (item) {
        idx = downloads.indexOf(CDownloadItem(item->id()));
    } else if (rpl) {
        idx = downloads.indexOf(CDownloadItem(rpl));
    }
    if (idx<0 || idx>=downloads.count())
        return;

    if (item) {
        downloads[idx].state = item->state();
    } else {
        downloads[idx].state = QWebEngineDownloadItem::DownloadCompleted;
    }

    downloads[idx].downloadItem = nullptr;

    if (rpl) {
        QFile f(downloads.at(idx).fileName);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(rpl->readAll());
            f.close();
        } else
            downloads[idx].state = QWebEngineDownloadItem::DownloadCancelled;

        rpl->deleteLater();
        downloads[idx].reply = nullptr;
    }

    Q_EMIT dataChanged(index(idx,0),index(idx,3));

    if (item)
        item->deleteLater();

    if (downloads.at(idx).autoDelete)
        deleteDownloadItem(index(idx,0));
}

void CDownloadsModel::downloadStateChanged(QWebEngineDownloadItem::DownloadState state)
{
    auto item = qobject_cast<QWebEngineDownloadItem *>(sender());
    if (item==nullptr) return;

    int idx = downloads.indexOf(CDownloadItem(item->id()));
    if (idx<0 || idx>=downloads.count()) return;

    downloads[idx].state = state;
    if (item->interruptReason()!=QWebEngineDownloadItem::NoReason)
        downloads[idx].errorString = item->interruptReasonString();

    Q_EMIT dataChanged(index(idx,0),index(idx,3));
}

void CDownloadsModel::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    auto item = qobject_cast<QWebEngineDownloadItem *>(sender());
    auto rpl = qobject_cast<QNetworkReply *>(sender());

    int idx = -1;

    if (item) {
        idx = downloads.indexOf(CDownloadItem(item->id()));
    } else if (rpl) {
        idx = downloads.indexOf(CDownloadItem(rpl));
    }
    if (idx<0 || idx>=downloads.count()) return;

    downloads[idx].received = bytesReceived;
    downloads[idx].total = bytesTotal;

    Q_EMIT dataChanged(index(idx,2),index(idx,3));
}

void CDownloadsModel::abortDownload()
{
    auto acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    bool ok = false;
    if (downloads.at(idx).state==QWebEngineDownloadItem::DownloadInProgress) {
        if (downloads.at(idx).downloadItem) {
            downloads[idx].downloadItem->cancel();
            ok = true;
        } else if (downloads.at(idx).reply) {
            downloads[idx].reply->abort();
            ok = true;
        }
    }

    if (!ok) {
        QMessageBox::warning(m_manager,tr("JPReader"),
                             tr("Unable to stop this download. Incorrect state."));
    }
}

void CDownloadsModel::cleanDownload()
{
    auto acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    bool ok = false;
    if (downloads.at(idx).state==QWebEngineDownloadItem::DownloadInProgress) {
        if (downloads.at(idx).downloadItem) {
            downloads[idx].autoDelete = true;
            downloads[idx].downloadItem->cancel();
            ok = true;
        } else if (downloads.at(idx).reply) {
            downloads[idx].autoDelete = true;
            downloads[idx].reply->abort();
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
    auto acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    QFileInfo fi(downloads.at(idx).fileName);

    if (!QProcess::startDetached(QStringLiteral("xdg-open"), QStringList() << fi.path()))
        QMessageBox::critical(m_manager, tr("JPReader"), tr("Unable to start browser."));
}

void CDownloadsModel::openHere()
{
    auto acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    static const QStringList acceptedMime
            = { QStringLiteral("text/plain"), QStringLiteral("text/html"), QStringLiteral("application/pdf"),
                QStringLiteral("image/jpeg"), QStringLiteral("image/png"), QStringLiteral("image/gif"),
                QStringLiteral("image/svg+xml"), QStringLiteral("image/webp") };

    static const QStringList acceptedExt
            = { QStringLiteral("pdf"), QStringLiteral("htm"), QStringLiteral("html"), QStringLiteral("txt"),
                QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("jpe"), QStringLiteral("png"),
                QStringLiteral("svg"), QStringLiteral("gif"), QStringLiteral("webp") };

    QString fname = downloads.at(idx).fileName;
    QString mime = downloads.at(idx).mimeType;
    QFileInfo fi(fname);
    if ((acceptedMime.contains(mime,Qt::CaseInsensitive) ||
         acceptedExt.contains(fi.suffix(),Qt::CaseInsensitive))
            && (gSet->activeWindow() != nullptr))
        new CSnippetViewer(gSet->activeWindow(),QUrl::fromLocalFile(fname));
}

void CDownloadsModel::openXdg()
{
    auto acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int idx = acm->data().toInt();
    if (idx<0 || idx>=downloads.count()) return;

    if (!QProcess::startDetached(QStringLiteral("xdg-open"), QStringList() << downloads.at(idx).fileName))
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
    downloadItem = nullptr;
    autoDelete = false;
    reply = nullptr;
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
    downloadItem = other.downloadItem;
    autoDelete = other.autoDelete;
    reply = other.reply;
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
    downloadItem = nullptr;
    autoDelete = false;
    reply = nullptr;
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
    downloadItem = nullptr;
    autoDelete = false;
    reply = rpl;
}

CDownloadItem::CDownloadItem(QWebEngineDownloadItem* item)
{
    if (item==nullptr) {
        id = 0;
        fileName.clear();
        mimeType.clear();
        errorString.clear();
        state = QWebEngineDownloadItem::DownloadRequested;
        received = 0;
        total = 0;
        downloadItem = nullptr;
        autoDelete = false;
        reply = nullptr;
    } else {
        id = item->id();
        fileName = item->path();
        mimeType = item->mimeType();
        errorString.clear();
        state = item->state();
        received = item->receivedBytes();
        total = item->totalBytes();
        downloadItem = item;
        autoDelete = false;
        reply = nullptr;
    }
}

CDownloadItem::CDownloadItem(QNetworkReply *rpl, const QString &fname)
{
    id = 0;
    fileName = fname;
    mimeType = QStringLiteral("application/download");
    errorString.clear();
    state = QWebEngineDownloadItem::DownloadInProgress;
    received = 0;
    total = 0;
    downloadItem = nullptr;
    autoDelete = false;
    reply = rpl;
}

bool CDownloadItem::operator==(const CDownloadItem &s) const
{
    if (id!=0 && s.id!=0)
        return (id==s.id);

    return reply==s.reply;
}

bool CDownloadItem::operator!=(const CDownloadItem &s) const
{
    return !operator==(s);
}

bool CDownloadItem::isEmpty()
{
    return (id==0 && reply==nullptr && downloadItem==nullptr && fileName.isEmpty());
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
        const int topMargin = 10;
        const int bottomMargin = 5;

        QStyleOptionProgressBar progressBarOption;
        QRect r = option.rect;
        r.setHeight(r.height()-topMargin); r.moveTop(r.top()+bottomMargin);
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
    Q_UNUSED(option)
    Q_UNUSED(index)

    const QSize defaultSize(200,32);
    return defaultSize;
}
