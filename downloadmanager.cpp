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

namespace CDefaults {
const char zipSeparator = 0x00;
}

CDownloadManager::CDownloadManager(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CDownloadManager)
{
    ui->setupUi(this);

    setWindowIcon(QIcon::fromTheme(QSL("folder-downloads")));

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

    QString fname;
    if (index>=0) {
        fname.append(QSL("%1_")
                     .arg(index,CGenericFuncs::numDigits(maxIndex),indexBase,QLatin1Char('0')));
    }
    fname.append(url.fileName());
    if (path.endsWith(QSL(".zip"),Qt::CaseInsensitive)) {
        fname = QSL("%1%2%3").arg(path).arg(CDefaults::zipSeparator).arg(fname);
    } else {
        fname = QDir(path).filePath(fname);
    }

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
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return Qt::NoItemFlags;

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

QVariant CDownloadsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    int idx = index.row();

    const QSize iconSize(16,16);

    CDownloadItem t = downloads.at(idx);
    QString zfname = t.getZipName();

    if (role == Qt::DecorationRole) {
        if (index.column()==0) {
            switch (t.state) {
                case QWebEngineDownloadItem::DownloadCancelled:
                    return QIcon::fromTheme(QSL("task-reject")).pixmap(iconSize);
                case QWebEngineDownloadItem::DownloadCompleted:
                    return QIcon::fromTheme(QSL("task-complete")).pixmap(iconSize);
                case QWebEngineDownloadItem::DownloadInProgress:
                case QWebEngineDownloadItem::DownloadRequested:
                    return QIcon::fromTheme(QSL("arrow-right")).pixmap(iconSize);
                case QWebEngineDownloadItem::DownloadInterrupted:
                    return QIcon::fromTheme(QSL("task-attention")).pixmap(iconSize);
            }
            return QVariant();
        }
    } else if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 1: {
                QFileInfo fi(t.getFileName());
                if (!zfname.isEmpty()) {
                    QFileInfo zfi(zfname);
                    return tr("%1 (zip: %2)").arg(fi.fileName(),zfi.fileName());
                }

                return fi.fileName();
            }
            case 2:
                if (t.total==0) return QSL("0%");
                return QSL("%1%").arg(100*t.received/t.total);
            case 3:
                if (!t.errorString.isEmpty())
                    return t.errorString;
                return QSL("%1 / %2")
                        .arg(CGenericFuncs::formatFileSize(t.received),
                             CGenericFuncs::formatFileSize(t.total));
            default: return QVariant();
        }
    } else if (role == Qt::ToolTipRole || role == Qt::StatusTipRole) {
        if (!t.errorString.isEmpty() && index.column()==0)
            return t.errorString;
        if (!zfname.isEmpty())
            return tr("%1\nZip: %2)").arg(t.getFileName(),zfname);
        return t.getFileName();
    } else if (role == Qt::UserRole+1) {
        if (t.total==0)
            return 0;
        return 100*t.received/t.total;
    }
    return QVariant();
}

QVariant CDownloadsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return QSL(" ");
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
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    return downloads.count();
}

int CDownloadsModel::columnCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    const int columnsCount = 4;
    return columnsCount;
}

CDownloadItem CDownloadsModel::getDownloadItem(const QModelIndex &index)
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid)) return CDownloadItem();

    return downloads.at(index.row());
}

void CDownloadsModel::deleteDownloadItem(const QModelIndex &index)
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid)) return;

    int row = index.row();
    beginRemoveRows(QModelIndex(),row,row);
    downloads.removeAt(row);
    endRemoveRows();
}

void CDownloadsModel::appendItem(const CDownloadItem &item)
{
    int row = downloads.count();
    beginInsertRows(QModelIndex(),row,row);
    downloads << item;
    endInsertRows();
}

void CDownloadsModel::downloadFinished()
{
    QScopedPointer<QWebEngineDownloadItem,QScopedPointerDeleteLater> item(qobject_cast<QWebEngineDownloadItem *>(sender()));
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));

    int row = -1;

    if (item) {
        row = downloads.indexOf(CDownloadItem(item->id()));
    } else if (rpl) {
        row = downloads.indexOf(CDownloadItem(rpl.data()));
    }
    if (row<0 || row>=downloads.count())
        return;

    if (item) {
        downloads[row].state = item->state();
    } else {
        downloads[row].state = QWebEngineDownloadItem::DownloadCompleted;
    }

    downloads[row].downloadItem = nullptr;

    if (rpl) {
        if (rpl->error()==QNetworkReply::NoError) {
            if (!downloads.at(row).getZipName().isEmpty()) {
                if (!CGenericFuncs::writeBytesToZip(downloads.at(row).getZipName(),
                                                    downloads.at(row).getFileName(),
                                                    rpl->readAll()))
                    downloads[row].state = QWebEngineDownloadItem::DownloadCancelled;
            } else {
                QFile f(downloads.at(row).getFileName()); // TODO: continuous file download for big files
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(rpl->readAll());
                    f.close();
                } else
                    downloads[row].state = QWebEngineDownloadItem::DownloadCancelled;
            }
        } else {
            downloads[row].state = QWebEngineDownloadItem::DownloadInterrupted;
            downloads[row].errorString = tr("Error %1: %2").arg(rpl->error()).arg(rpl->errorString());
        }
        downloads[row].reply = nullptr;
    }

    Q_EMIT dataChanged(index(row,0),index(row,3));

    if (downloads.at(row).autoDelete)
        deleteDownloadItem(index(row,0));
}

void CDownloadsModel::downloadStateChanged(QWebEngineDownloadItem::DownloadState state)
{
    auto item = qobject_cast<QWebEngineDownloadItem *>(sender());
    if (item==nullptr) return;

    int row = downloads.indexOf(CDownloadItem(item->id()));
    if (row<0 || row>=downloads.count()) return;

    downloads[row].state = state;
    if (item->interruptReason()!=QWebEngineDownloadItem::NoReason)
        downloads[row].errorString = item->interruptReasonString();

    Q_EMIT dataChanged(index(row,0),index(row,3));
}

void CDownloadsModel::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    auto item = qobject_cast<QWebEngineDownloadItem *>(sender());
    auto rpl = qobject_cast<QNetworkReply *>(sender());

    int row = -1;

    if (item) {
        row = downloads.indexOf(CDownloadItem(item->id()));
    } else if (rpl) {
        row = downloads.indexOf(CDownloadItem(rpl));
    }
    if (row<0 || row>=downloads.count()) return;

    downloads[row].received = bytesReceived;
    downloads[row].total = bytesTotal;

    Q_EMIT dataChanged(index(row,2),index(row,3));
}

void CDownloadsModel::abortDownload()
{
    auto acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=downloads.count()) return;

    bool ok = false;
    if (downloads.at(row).state==QWebEngineDownloadItem::DownloadInProgress) {
        if (downloads.at(row).downloadItem) {
            downloads[row].downloadItem->cancel();
            ok = true;
        } else if (downloads.at(row).reply) {
            downloads[row].reply->abort();
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

    int row = acm->data().toInt();
    if (row<0 || row>=downloads.count()) return;

    bool ok = false;
    if (downloads.at(row).state==QWebEngineDownloadItem::DownloadInProgress) {
        if (downloads.at(row).downloadItem) {
            downloads[row].autoDelete = true;
            downloads[row].downloadItem->cancel();
            ok = true;
        } else if (downloads.at(row).reply) {
            downloads[row].autoDelete = true;
            downloads[row].reply->abort();
            ok = true;
        }
    }
    if (!ok)
        deleteDownloadItem(index(row,0));
}

void CDownloadsModel::cleanFinishedDownloads()
{
    int row = 0;
    while (row<downloads.count()) {
        if (downloads.at(row).state!=QWebEngineDownloadItem::DownloadInProgress) {
            beginRemoveRows(QModelIndex(),row,row);
            downloads.removeAt(row);
            endRemoveRows();
            row = 0;
        } else
            row++;
    }
}

void CDownloadsModel::openDirectory()
{
    auto acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=downloads.count()) return;

    QString fname = downloads.at(row).getZipName();
    if (fname.isEmpty())
        fname = downloads.at(row).getFileName();
    QFileInfo fi(fname);

    if (!QProcess::startDetached(QSL("xdg-open"), QStringList() << fi.path()))
        QMessageBox::critical(m_manager, tr("JPReader"), tr("Unable to start browser."));
}

void CDownloadsModel::openHere()
{
    auto acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=downloads.count()) return;

    static const QStringList acceptedMime
            = { QSL("text/plain"), QSL("text/html"), QSL("application/pdf"),
                QSL("image/jpeg"), QSL("image/png"), QSL("image/gif"),
                QSL("image/svg+xml"), QSL("image/webp") };

    static const QStringList acceptedExt
            = { QSL("pdf"), QSL("htm"), QSL("html"), QSL("txt"),
                QSL("jpg"), QSL("jpeg"), QSL("jpe"), QSL("png"),
                QSL("svg"), QSL("gif"), QSL("webp") };

    const QString fname = downloads.at(row).getFileName();
    const QString mime = downloads.at(row).mimeType;
    const QString zipName = downloads.at(row).getZipName();
    QFileInfo fi(fname);
    if (zipName.isEmpty() &&
            (acceptedMime.contains(mime,Qt::CaseInsensitive) ||
             acceptedExt.contains(fi.suffix(),Qt::CaseInsensitive))
            && (gSet->activeWindow() != nullptr))
        new CSnippetViewer(gSet->activeWindow(),QUrl::fromLocalFile(fname));
}

void CDownloadsModel::openXdg()
{
    auto acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=downloads.count()) return;
    if (!downloads.at(row).getZipName().isEmpty()) return;

    if (!QProcess::startDetached(QSL("xdg-open"), QStringList() << downloads.at(row).getFileName()))
        QMessageBox::critical(m_manager, tr("JPReader"), tr("Unable to open application."));
}

CDownloadItem::CDownloadItem(quint32 itemId)
{
    id = itemId;
}

CDownloadItem::CDownloadItem(QNetworkReply *rpl)
{
    reply = rpl;
}

CDownloadItem::CDownloadItem(QWebEngineDownloadItem* item)
{
    if (item!=nullptr) {
        id = item->id();
        pathName = item->path();
        mimeType = item->mimeType();
        errorString.clear();
        state = item->state();
        received = item->receivedBytes();
        total = item->totalBytes();
        downloadItem = item;
    }
}

CDownloadItem::CDownloadItem(QNetworkReply *rpl, const QString &fname)
{
    pathName = fname;
    mimeType = QSL("application/download");
    state = QWebEngineDownloadItem::DownloadInProgress;
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
    return (id==0 && reply==nullptr && downloadItem==nullptr && pathName.isEmpty());
}

QString CDownloadItem::getFileName() const
{
    if (pathName.contains(CDefaults::zipSeparator))
        return pathName.split(CDefaults::zipSeparator).last();

    return pathName;
}

QString CDownloadItem::getZipName() const
{
    if (pathName.contains(CDefaults::zipSeparator))
        return pathName.split(CDefaults::zipSeparator).first();

    return QString();
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
