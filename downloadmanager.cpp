#include <QWidget>
#include <QCloseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPainter>
#include <QClipboard>
#include <QDebug>
#include "downloadmanager.h"
#include "genericfuncs.h"
#include "globalcontrol.h"
#include "downloadwriter.h"
#include "snviewer.h"
#include "ui_downloadmanager.h"

namespace CDefaults {
const char zipSeparator = 0x00;
const int writerStatusTimerIntervalMS = 250;
const int writerStatusLabelSize = 24;
}

CDownloadManager::CDownloadManager(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CDownloadManager)
{
    ui->setupUi(this);

    setWindowIcon(QIcon::fromTheme(QSL("folder-downloads")));

    m_model = new CDownloadsModel(this);
    ui->tableDownloads->setModel(m_model);
    ui->tableDownloads->verticalHeader()->hide();
    ui->tableDownloads->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableDownloads->setItemDelegateForColumn(2,new CDownloadBarDelegate(this,m_model));

    ui->labelWriter->setPixmap(QIcon::fromTheme(QSL("document-save")).pixmap(CDefaults::writerStatusLabelSize));
    QSizePolicy sp = ui->labelWriter->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    ui->labelWriter->setSizePolicy(sp);
    ui->labelWriter->hide();

    connect(ui->tableDownloads, &QTableView::customContextMenuRequested,
            this, &CDownloadManager::contextMenu);

    connect(ui->buttonClearFinished, &QPushButton::clicked,
            m_model, &CDownloadsModel::cleanFinishedDownloads);

    connect(ui->buttonClearDownloaded, &QPushButton::clicked,
            m_model, &CDownloadsModel::cleanCompletedDownloads);

    connect(ui->buttonAbortAll, &QPushButton::clicked,
            m_model, &CDownloadsModel::abortAll);

    connect(&m_writerStatusTimer, &QTimer::timeout,
            this, &CDownloadManager::updateWriterStatus);
    m_writerStatusTimer.setInterval(CDefaults::writerStatusTimerIntervalMS);
    m_writerStatusTimer.start();
}

CDownloadManager::~CDownloadManager()
{
    delete ui;
}

void CDownloadManager::handleAuxDownload(const QString& src, const QString& suggestedFilename,
                                         const QString& containerPath, const QUrl& referer,
                                         int index, int maxIndex, bool isFanbox, bool relaxedRedirects)
{
    QUrl url(src);
    if (!url.isValid() || url.isRelative()) return;

    QString fname;
    if (index>=0)
        fname.append(QSL("%1_").arg(CGenericFuncs::paddedNumber(index,maxIndex)));
    if (suggestedFilename.isEmpty()) {
        fname.append(url.fileName());
    } else {
        fname.append(suggestedFilename);
    }
    if (containerPath.endsWith(QSL(".zip"),Qt::CaseInsensitive)) {
        fname = QSL("%1%2%3").arg(containerPath).arg(CDefaults::zipSeparator).arg(fname);
    } else {
        fname = QDir(containerPath).filePath(fname);
    }

    if (!isVisible())
        show();

    if (fname.isNull() || fname.isEmpty()) return;
    gSet->setSavedAuxSaveDir(containerPath);

    QNetworkRequest req(url);
    req.setRawHeader("referer",referer.toString().toUtf8());
    if (isFanbox)
        req.setRawHeader("origin","https://www.fanbox.cc");
    if (relaxedRedirects) {
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::UserVerifiedRedirectPolicy);
    } else {
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);
    }
    req.setMaximumRedirectsAllowed(CDefaults::httpMaxRedirects);
    QNetworkReply* rpl = gSet->auxNetworkAccessManagerGet(req);

    connect(rpl, &QNetworkReply::finished,
            m_model, &CDownloadsModel::downloadFinished);
    connect(rpl, &QNetworkReply::downloadProgress,
            m_model, &CDownloadsModel::downloadProgress);
    if (relaxedRedirects) {
        connect(rpl,&QNetworkReply::redirected,rpl,[rpl,url](const QUrl& rurl) {

            // redirect rules
            if (url.host().endsWith(QSL("patreon.com"),Qt::CaseInsensitive) &&
                    (rurl.host().endsWith(QSL(".patreon.com"),Qt::CaseInsensitive) ||
                     rurl.host().endsWith(QSL(".patreonusercontent.com")))) {

                Q_EMIT rpl->redirectAllowed();
            }
        });
    }

    m_model->appendItem(CDownloadItem(rpl, fname));
}

void CDownloadManager::setProgressLabel(const QString &text)
{
    ui->labelProgress->setText(text);
}

void CDownloadManager::handleDownload(QWebEngineDownloadItem *item)
{
    if (item==nullptr) return;

    if (!isVisible())
        show();

    if (item->savePageFormat()==QWebEngineDownloadItem::UnknownSaveFormat) {
        // Async save request from user, not full html page
        QString fname = CGenericFuncs::getSaveFileNameD(this,tr("Save file"),gSet->settings()->savedAuxSaveDir,
                                                        QStringList(),nullptr,item->suggestedFileName());

        if (fname.isNull() || fname.isEmpty()) {
            item->cancel();
            item->deleteLater();
            return;
        }

        QFileInfo fi(fname);
        gSet->setSavedAuxSaveDir(fi.absolutePath());
        item->setDownloadDirectory(fi.absolutePath());
        item->setDownloadFileName(fi.fileName());
    }

    connect(item, &QWebEngineDownloadItem::finished,
            m_model, &CDownloadsModel::downloadFinished);
    connect(item, &QWebEngineDownloadItem::downloadProgress,
            m_model, &CDownloadsModel::downloadProgress);
    connect(item, &QWebEngineDownloadItem::stateChanged,
            m_model, &CDownloadsModel::downloadStateChanged);

    m_model->appendItem(CDownloadItem(item));

    item->accept();
}

void CDownloadManager::contextMenu(const QPoint &pos)
{
    QModelIndex idx = ui->tableDownloads->indexAt(pos);
    CDownloadItem item = m_model->getDownloadItem(idx);
    if (item.isEmpty()) return;

    QMenu cm;
    QAction *acm = nullptr;

    if (item.url.isValid()) {
        acm = cm.addAction(tr("Copy URL to clipboard"),m_model,&CDownloadsModel::copyUrlToClipboard);
        acm->setData(idx.row());
        cm.addSeparator();
    }

    if (item.state==QWebEngineDownloadItem::DownloadInProgress) {
        acm = cm.addAction(tr("Abort"),m_model,&CDownloadsModel::abortDownload);
        acm->setData(idx.row());
        cm.addSeparator();
    }

    acm = cm.addAction(tr("Remove"),m_model,&CDownloadsModel::cleanDownload);
    acm->setData(idx.row());

    if (item.state==QWebEngineDownloadItem::DownloadCompleted ||
            item.state==QWebEngineDownloadItem::DownloadInProgress) {
        cm.addSeparator();
        acm = cm.addAction(tr("Open here"),m_model,&CDownloadsModel::openHere);
        acm->setData(idx.row());
        acm = cm.addAction(tr("Open with associated application"),m_model,&CDownloadsModel::openXdg);
        acm->setData(idx.row());
        acm = cm.addAction(tr("Open containing directory"),m_model,&CDownloadsModel::openDirectory);
        acm->setData(idx.row());
    }

    cm.exec(ui->tableDownloads->mapToGlobal(pos));
}

qint64 CDownloadManager::receivedBytes() const
{
    return m_receivedBytes;
}

void CDownloadManager::addReceivedBytes(qint64 size)
{
    m_receivedBytes += size;
}

void CDownloadManager::updateWriterStatus()
{
    auto *writer = gSet->downloadWriter();
    if (writer)
        ui->labelWriter->setVisible(writer->getWorkCount()>0);
}

void CDownloadManager::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void CDownloadManager::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    m_writerStatusTimer.start();

    if (!m_firstResize) return;
    m_firstResize = false;

    const int filenameColumnWidth = 350;
    const int completionColumnWidth = 200;

    ui->tableDownloads->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    ui->tableDownloads->setColumnWidth(1,filenameColumnWidth);
    ui->tableDownloads->setColumnWidth(2,completionColumnWidth);
    ui->tableDownloads->horizontalHeader()->setStretchLastSection(true);
}

void CDownloadManager::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event)
    m_writerStatusTimer.stop();
}

void CDownloadsModel::updateProgressLabel()
{
    if (m_downloads.isEmpty()) {
        m_manager->setProgressLabel(QString());
        return;
    }

    int total = m_downloads.count();
    int completed = 0;
    int failures = 0;
    int cancelled = 0;
    int active = 0;
    for(const auto &item : qAsConst(m_downloads)) {
        switch (item.state) {
            case QWebEngineDownloadItem::DownloadCancelled:
                cancelled++;
                break;
            case QWebEngineDownloadItem::DownloadCompleted:
                completed++;
                break;
            case QWebEngineDownloadItem::DownloadInProgress:
            case QWebEngineDownloadItem::DownloadRequested:
                active++;
                break;
            case QWebEngineDownloadItem::DownloadInterrupted:
                failures++;
                break;
        }
    }

    m_manager->setProgressLabel(tr("Downloaded: %1 of %2. Active: %3. Failures: %4. Cancelled %5.\n"
                                   "Total downloaded: %6.")
                                .arg(completed)
                                .arg(total)
                                .arg(active)
                                .arg(failures)
                                .arg(cancelled)
                                .arg(CGenericFuncs::formatFileSize(m_manager->receivedBytes())));
}

CDownloadsModel::CDownloadsModel(CDownloadManager *parent)
    : QAbstractTableModel(parent)
{
    m_manager = parent;
    m_downloads.clear();
    connect(this,&CDownloadsModel::writeBytesToZip,
            gSet->downloadWriter(),&CDownloadWriter::writeBytesToZip,Qt::QueuedConnection);
    connect(this,&CDownloadsModel::writeBytesToFile,
            gSet->downloadWriter(),&CDownloadWriter::writeBytesToFile,Qt::QueuedConnection);
    connect(gSet->downloadWriter(),&CDownloadWriter::writeComplete,
            this,&CDownloadsModel::writerCompleted,Qt::QueuedConnection);
    connect(gSet->downloadWriter(),&CDownloadWriter::error,
            this,&CDownloadsModel::writerError,Qt::QueuedConnection);
    updateProgressLabel();
}

CDownloadsModel::~CDownloadsModel()
{
    m_downloads.clear();
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

    CDownloadItem t = m_downloads.at(idx);
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
                if (t.total<0) return QString();
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
        if (t.total<=0) return t.total;
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

    return m_downloads.count();
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

    return m_downloads.at(index.row());
}

void CDownloadsModel::deleteDownloadItem(const QModelIndex &index)
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid)) return;

    int row = index.row();
    beginRemoveRows(QModelIndex(),row,row);
    m_downloads.removeAt(row);
    endRemoveRows();
    updateProgressLabel();
}

void CDownloadsModel::appendItem(const CDownloadItem &item)
{
    int row = m_downloads.count();
    beginInsertRows(QModelIndex(),row,row);
    m_downloads << item;
    endInsertRows();
    updateProgressLabel();
}

void CDownloadsModel::downloadFinished()
{
    QScopedPointer<QWebEngineDownloadItem,QScopedPointerDeleteLater> item(qobject_cast<QWebEngineDownloadItem *>(sender()));
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));

    int row = -1;

    if (item) {
        row = m_downloads.indexOf(CDownloadItem(item->id()));
    } else if (rpl) {
        row = m_downloads.indexOf(CDownloadItem(rpl.data()));
    }
    if (row<0 || row>=m_downloads.count())
        return;

    if (item)
        m_downloads[row].state = item->state();

    if (rpl) {
        if (rpl->error()==QNetworkReply::NoError) {
            QUuid uuid = m_downloads.at(row).auxId;
            if (!m_downloads.at(row).getZipName().isEmpty()) {
                Q_EMIT writeBytesToZip(m_downloads.at(row).getZipName(),
                                       m_downloads.at(row).getFileName(),
                                       rpl->readAll(),uuid);
            } else {
                Q_EMIT writeBytesToFile(m_downloads.at(row).getFileName(),rpl->readAll(),uuid);
            }
        } else {
            m_downloads[row].state = QWebEngineDownloadItem::DownloadInterrupted;
            m_downloads[row].errorString = tr("Error %1: %2").arg(rpl->error()).arg(rpl->errorString());
        }
        m_downloads[row].reply = nullptr;

    } else if (item) {

        m_downloads[row].downloadItem = nullptr;
    }

    Q_EMIT dataChanged(index(row,0),index(row,3)); // NOLINT

    updateProgressLabel();

    if ((m_downloads.at(row).autoDelete) ||
            (item && (item->state() == QWebEngineDownloadItem::DownloadCompleted)
             && gSet->settings()->downloaderCleanCompleted)) {

        deleteDownloadItem(index(row,0));
    }
}

void CDownloadsModel::downloadStateChanged(QWebEngineDownloadItem::DownloadState state)
{
    auto *item = qobject_cast<QWebEngineDownloadItem *>(sender());
    if (item==nullptr) return;

    int row = m_downloads.indexOf(CDownloadItem(item->id()));
    if (row<0 || row>=m_downloads.count()) return;

    m_downloads[row].state = state;
    if (item->interruptReason()!=QWebEngineDownloadItem::NoReason)
        m_downloads[row].errorString = item->interruptReasonString();

    updateProgressLabel();

    Q_EMIT dataChanged(index(row,0),index(row,3));
}

void CDownloadsModel::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    auto *item = qobject_cast<QWebEngineDownloadItem *>(sender());
    auto *rpl = qobject_cast<QNetworkReply *>(sender());

    int row = -1;

    if (item) {
        row = m_downloads.indexOf(CDownloadItem(item->id()));
    } else if (rpl) {
        row = m_downloads.indexOf(CDownloadItem(rpl));
    }
    if (row<0 || row>=m_downloads.count()) return;

    m_downloads[row].oldReceived = m_downloads.at(row).received;
    m_downloads[row].received = bytesReceived;
    m_downloads[row].total = bytesTotal;

    m_manager->addReceivedBytes(m_downloads.at(row).received
                                - m_downloads.at(row).oldReceived);

    updateProgressLabel();

    Q_EMIT dataChanged(index(row,2),index(row,3));
}

void CDownloadsModel::abortDownload()
{
    auto *acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=m_downloads.count()) return;

    if (!abortDownloadPriv(row)) {
        QMessageBox::warning(m_manager,tr("JPReader"),
                             tr("Unable to stop this download. Incorrect state."));
    }
}

void CDownloadsModel::abortAll()
{
    for (int i=0;i<m_downloads.count();i++)
        abortDownloadPriv(i);
}

bool CDownloadsModel::abortDownloadPriv(int row)
{
    bool res = false;
    if (m_downloads.at(row).state==QWebEngineDownloadItem::DownloadInProgress) {
        if (m_downloads.at(row).downloadItem) {
            m_downloads[row].downloadItem->cancel();
            res = true;
        } else if (m_downloads.at(row).reply) {
            m_downloads[row].reply->abort();
            res = true;
        }
    }

    if (res)
        updateProgressLabel();

    return res;
}

void CDownloadsModel::cleanDownload()
{
    auto *acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=m_downloads.count()) return;

    bool ok = false;
    if (m_downloads.at(row).state==QWebEngineDownloadItem::DownloadInProgress) {
        if (m_downloads.at(row).downloadItem) {
            m_downloads[row].autoDelete = true;
            m_downloads[row].downloadItem->cancel();
            ok = true;
        } else if (m_downloads.at(row).reply) {
            m_downloads[row].autoDelete = true;
            m_downloads[row].reply->abort();
            ok = true;
        }
    }
    if (!ok)
        deleteDownloadItem(index(row,0));
    updateProgressLabel();
}

void CDownloadsModel::copyUrlToClipboard()
{
    auto *acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=m_downloads.count()) return;

    QClipboard *cb = QApplication::clipboard();
    cb->setText(m_downloads.at(row).url.toString(),QClipboard::Clipboard);
}

void CDownloadsModel::cleanFinishedDownloads()
{
    int row = 0;
    while (row<m_downloads.count()) {
        if (m_downloads.at(row).state!=QWebEngineDownloadItem::DownloadInProgress) {
            beginRemoveRows(QModelIndex(),row,row);
            m_downloads.removeAt(row);
            endRemoveRows();
            row = 0;
        } else
            row++;
    }
    updateProgressLabel();
}

void CDownloadsModel::cleanCompletedDownloads()
{
    int row = 0;
    while (row<m_downloads.count()) {
        if (m_downloads.at(row).state==QWebEngineDownloadItem::DownloadCompleted) {
            beginRemoveRows(QModelIndex(),row,row);
            m_downloads.removeAt(row);
            endRemoveRows();
            row = 0;
        } else
            row++;
    }
    updateProgressLabel();
}

void CDownloadsModel::openDirectory()
{
    auto *acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=m_downloads.count()) return;

    QString fname = m_downloads.at(row).getZipName();
    if (fname.isEmpty())
        fname = m_downloads.at(row).getFileName();
    QFileInfo fi(fname);

    if (!QProcess::startDetached(QSL("xdg-open"), QStringList() << fi.path()))
        QMessageBox::critical(m_manager, tr("JPReader"), tr("Unable to start browser."));
}

void CDownloadsModel::openHere()
{
    auto *acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=m_downloads.count()) return;

    static const QStringList acceptedMime
            = { QSL("text/plain"), QSL("text/html"), QSL("application/pdf"),
                QSL("image/jpeg"), QSL("image/png"), QSL("image/gif"),
                QSL("image/svg+xml"), QSL("image/webp") };

    static const QStringList acceptedExt
            = { QSL("pdf"), QSL("htm"), QSL("html"), QSL("txt"),
                QSL("jpg"), QSL("jpeg"), QSL("jpe"), QSL("png"),
                QSL("svg"), QSL("gif"), QSL("webp") };

    const QString fname = m_downloads.at(row).getFileName();
    const QString mime = m_downloads.at(row).mimeType;
    const QString zipName = m_downloads.at(row).getZipName();
    QFileInfo fi(fname);
    if (zipName.isEmpty() &&
            (acceptedMime.contains(mime,Qt::CaseInsensitive) ||
             acceptedExt.contains(fi.suffix(),Qt::CaseInsensitive))
            && (gSet->activeWindow() != nullptr))
        new CSnippetViewer(gSet->activeWindow(),QUrl::fromLocalFile(fname));
}

void CDownloadsModel::openXdg()
{
    auto *acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=m_downloads.count()) return;
    if (!m_downloads.at(row).getZipName().isEmpty()) return;

    if (!QProcess::startDetached(QSL("xdg-open"), QStringList() << m_downloads.at(row).getFileName()))
        QMessageBox::critical(m_manager, tr("JPReader"), tr("Unable to open application."));
}

void CDownloadsModel::writerError(const QString &message, const QUuid &uuid)
{
    int row = m_downloads.indexOf(CDownloadItem(uuid));
    if (row<0 || row>=m_downloads.count())
        return;

    m_downloads[row].state = QWebEngineDownloadItem::DownloadCancelled;
    m_downloads[row].errorString = message;
    updateProgressLabel();

    Q_EMIT dataChanged(index(row,0),index(row,3));
}

void CDownloadsModel::writerCompleted(const QUuid &uuid)
{
    int row = m_downloads.indexOf(CDownloadItem(uuid));
    if (row<0 || row>=m_downloads.count())
        return;

    m_downloads[row].state = QWebEngineDownloadItem::DownloadCompleted;
    updateProgressLabel();

    Q_EMIT dataChanged(index(row,0),index(row,3));

    if (gSet->settings()->downloaderCleanCompleted)
        deleteDownloadItem(index(row,0));
}

CDownloadItem::CDownloadItem(quint32 itemId)
{
    id = itemId;
}

CDownloadItem::CDownloadItem(QNetworkReply *rpl)
{
    reply = rpl;
    url = rpl->url();
    auxId = QUuid::createUuid();
}

CDownloadItem::CDownloadItem(QWebEngineDownloadItem* item)
{
    if (item!=nullptr) {
        id = item->id();
        pathName = QSL("%1%2%3").arg(item->downloadDirectory(),
                                     QDir::separator(),
                                     item->downloadFileName());
        mimeType = item->mimeType();
        errorString.clear();
        state = item->state();
        received = item->receivedBytes();
        total = item->totalBytes();
        downloadItem = item;
        url = item->url();
    }
}

CDownloadItem::CDownloadItem(const QUuid &uuid)
{
    auxId = uuid;
}

CDownloadItem::CDownloadItem(QNetworkReply *rpl, const QString &fname)
{
    pathName = fname;
    mimeType = QSL("application/download");
    state = QWebEngineDownloadItem::DownloadInProgress;
    reply = rpl;
    url = rpl->url();
    auxId = QUuid::createUuid();
}

bool CDownloadItem::operator==(const CDownloadItem &s) const
{
    if (id!=0 && s.id!=0)
        return (id==s.id);

    if (reply!=nullptr && s.reply!=nullptr)
        return reply==s.reply;

    return auxId==s.auxId;
}

bool CDownloadItem::operator!=(const CDownloadItem &s) const
{
    return !operator==(s);
}

bool CDownloadItem::isEmpty() const
{
    return (id==0 && reply==nullptr && auxId.isNull() && downloadItem==nullptr && pathName.isEmpty());
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
        if (progress>=0) {
            progressBarOption.minimum = 0;
            progressBarOption.maximum = 100;
            progressBarOption.progress = progress;
            progressBarOption.text = QString::number(progress) + "%";
            progressBarOption.textVisible = true;
        } else {
            progressBarOption.minimum = 0;
            progressBarOption.maximum = 0;
            progressBarOption.textVisible = false;
        }
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
