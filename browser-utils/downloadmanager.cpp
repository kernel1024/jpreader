#include <QWidget>
#include <QCloseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPainter>
#include <QClipboard>
#include <QDebug>
#include "downloadmanager.h"
#include "downloadwriter.h"
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/startup.h"
#include "global/ui.h"
#include "global/network.h"
#include "browser/browser.h"
#include "ui_downloadmanager.h"

namespace CDefaults {
const char zipSeparator = 0x00;
const int writerStatusTimerIntervalMS = 2000;
const int writerStatusLabelSize = 24;
const int downloadManagerColumnCount = 4;
const int maxZipWriterErrorsInteractive = 5;
const auto replyAuxId = "replyAuxID";
const auto replyHeadFileName = "replyHeadFileName";
const auto replyHeadOffset = "replyHeadOffset";
}

CDownloadManager::CDownloadManager(QWidget *parent, CZipWriter *zipWriter) :
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

    connect(zipWriter, &CZipWriter::zipError,
            this, &CDownloadManager::zipWriterError);

    m_writerStatusTimer.setInterval(CDefaults::writerStatusTimerIntervalMS);
    m_writerStatusTimer.start();
}

CDownloadManager::~CDownloadManager()
{
    delete ui;
}

bool CDownloadManager::handleAuxDownload(const QString& src, const QString& suggestedFilename,
                                         const QString& containerPath, const QUrl& referer,
                                         int index, int maxIndex, bool isFanbox, bool isPatreon,
                                         bool &forceOverwrite)
{
    if (!isVisible())
        show();

    QUrl url(src);
    if (!url.isValid() || url.isRelative()) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Download URL is invalid."));
        return false;
    }

    bool isZipTarget = false;
    QString fname;
    if (!computeFileName(fname,isZipTarget,index,maxIndex,url,containerPath,suggestedFilename)) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Computed file name is empty."));
        return false;
    }

    gSet->ui()->setSavedAuxSaveDir(containerPath);

    qint64 offset = 0L;
    if (!isZipTarget) {
        QFileInfo fi(fname);
        if (fi.exists() && !forceOverwrite) {
            QMessageBox mbox;
            mbox.setWindowTitle(QGuiApplication::applicationDisplayName());
            mbox.setText(tr("File %1 exists.").arg(fi.fileName()));
            mbox.setDetailedText(tr("Full path: %1").arg(fname));
            QPushButton *btnOverwrite = mbox.addButton(tr("Overwrite"),QMessageBox::YesRole);
            QPushButton *btnOverwriteAll = mbox.addButton(tr("Overwrite all"),QMessageBox::AcceptRole);
            QPushButton *btnResume = mbox.addButton(tr("Resume download"),QMessageBox::NoRole);
            QPushButton *btnAbort = mbox.addButton(QMessageBox::Abort);
            mbox.setDefaultButton(btnOverwriteAll);
            mbox.exec();
            if (mbox.clickedButton() == btnOverwrite) {
                offset = 0L;
            } else if (mbox.clickedButton() == btnOverwriteAll) {
                forceOverwrite = true;
            } else if (mbox.clickedButton() == btnResume) {
                offset = fi.size();
            } else if (mbox.clickedButton() == btnAbort) {
                return false;
            }
        }
    }

    // create common request (for HEAD and GET)
    QNetworkRequest req(url);
    req.setRawHeader("referer",referer.toString().toUtf8());
    if (isFanbox)
        req.setRawHeader("origin","https://www.fanbox.cc");
    if (isPatreon) {
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::UserVerifiedRedirectPolicy);
    } else {
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);
    }
    req.setMaximumRedirectsAllowed(CDefaults::httpMaxRedirects);

    // we need HEAD request for file size calculation
    if (offset > 0L) {
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerHead(req);
        rpl->setProperty(CDefaults::replyHeadFileName,fname);
        rpl->setProperty(CDefaults::replyHeadOffset,offset);
        connect(rpl,&QNetworkReply::errorOccurred,this,&CDownloadManager::headRequestFailed);
        connect(rpl,&QNetworkReply::finished,this,&CDownloadManager::headRequestFinished);
        if (rpl->request().attribute(QNetworkRequest::RedirectPolicyAttribute).toInt()
                == QNetworkRequest::UserVerifiedRedirectPolicy) {
            connect(rpl,&QNetworkReply::redirected,m_model,&CDownloadsModel::requestRedirected);
        }
        return true;
    }

    // download file from start
    m_model->createDownloadForNetworkRequest(req,fname,offset);
    return true;
}

void CDownloadsModel::createDownloadForNetworkRequest(const QNetworkRequest &request, const QString &fileName,
                                                     qint64 offset)
{
    QNetworkRequest req = request;
    req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute,true);
    QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

    appendItem(CDownloadItem(rpl, fileName, offset));

    connect(rpl, &QNetworkReply::finished,
            this, &CDownloadsModel::downloadFinished);
    connect(rpl, &QNetworkReply::downloadProgress,
            this, &CDownloadsModel::downloadProgress);

    if (rpl->request().attribute(QNetworkRequest::RedirectPolicyAttribute).toInt()
            == QNetworkRequest::UserVerifiedRedirectPolicy) {
        connect(rpl,&QNetworkReply::redirected,this,&CDownloadsModel::requestRedirected);
    }

    connect(rpl, &QNetworkReply::readyRead, [this,rpl](){

        int httpStatus = CGenericFuncs::getHttpStatusFromReply(rpl);

        if ((rpl->error() == QNetworkReply::NoError) && (httpStatus<CDefaults::httpCodeRedirect)) {
            const QUuid id = rpl->property(CDefaults::replyAuxId).toUuid();
            int idx = m_downloads.indexOf(CDownloadItem(id));
            Q_ASSERT(idx>=0);
            if (m_downloads.at(idx).writer.isNull())
                makeWriterJob(m_downloads[idx]);

            const QByteArray data = rpl->readAll();
            QPointer<CDownloadWriter> writer(m_downloads.at(idx).writer);
            QMetaObject::invokeMethod(writer, [writer,data](){
                writer->appendBytesToFile(data);
            }, Qt::QueuedConnection);
        }
    });
}

void CDownloadManager::headRequestFinished()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;

    int httpStatus = CGenericFuncs::getHttpStatusFromReply(rpl.data());

    qint64 length = -1L;
    if ((rpl->error() == QNetworkReply::NoError) && (httpStatus<CDefaults::httpCodeRedirect)) {
        bool ok = false;
        length = rpl->header(QNetworkRequest::ContentLengthHeader).toLongLong(&ok);
    }

    QNetworkRequest req = rpl->request();
    const QString fileName = rpl->property(CDefaults::replyHeadFileName).toString();
    const qint64 offset = rpl->property(CDefaults::replyHeadOffset).toLongLong();

    // Range header
    QString range = QSL("bytes=%1-")
                    .arg(offset);
    if (length>=0)
        range.append(QSL("%1").arg(length));
    req.setRawHeader("Range", range.toLatin1());

    // download file from offset
    m_model->createDownloadForNetworkRequest(req,fileName,offset);
}

void CDownloadManager::headRequestFailed(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;

    QNetworkRequest req = rpl->request();
    const QString fileName = rpl->property(CDefaults::replyHeadFileName).toString();

    qWarning() << tr("HEAD request failed for URL %1, %2.")
                  .arg(req.url().toString(),rpl->errorString());

    // HEAD request failed, assume simple download from start
    m_model->createDownloadForNetworkRequest(req,fileName,0L);
}

void CDownloadManager::zipWriterError(const QString &message)
{
    const QString msg(QSL("ZIP writer error: %1").arg(message));
    qCritical() << msg;

    m_zipErrorCount++;
    if (m_zipErrorCount>CDefaults::maxZipWriterErrorsInteractive) return;

    QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),msg);
}

void CDownloadsModel::requestRedirected(const QUrl &url)
{
    QPointer<QNetworkReply> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;

    // redirect rules
    if (url.host().endsWith(QSL("patreon.com"),Qt::CaseInsensitive) &&
            (rpl->request().url().host().endsWith(QSL(".patreon.com"),Qt::CaseInsensitive) ||
             rpl->request().url().host().endsWith(QSL(".patreonusercontent.com")))) {

        Q_EMIT rpl->redirectAllowed();
    }
}

bool CDownloadManager::computeFileName(QString &fileName, bool &isZipTarget,
                                       int index, int maxIndex,
                                       const QUrl &url, const QString &containerPath,
                                       const QString &suggestedFilename) const
{
    if (index>=0)
        fileName.append(QSL("%1_").arg(CGenericFuncs::paddedNumber(index,maxIndex)));
    if (suggestedFilename.isEmpty()) {
        fileName.append(CGenericFuncs::decodeHtmlEntities(url.fileName()));
    } else {
        fileName.append(suggestedFilename);
    }
    if (containerPath.endsWith(QSL(".zip"),Qt::CaseInsensitive)) {
        fileName = QSL("%1%2%3").arg(containerPath).arg(CDefaults::zipSeparator).arg(fileName);
        isZipTarget = true;
    } else {
        fileName = QDir(containerPath).filePath(fileName);
    }

    return (!(fileName.isEmpty()));
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
        gSet->ui()->setSavedAuxSaveDir(fi.absolutePath());
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
    ui->labelWriter->setVisible((CDownloadWriter::getWorkCount()>0) ||
                                CZipWriter::isBusy());
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
    m_manager->updateWriterStatus();
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
                case QWebEngineDownloadItem::DownloadRequested:
                    return QIcon::fromTheme(QSL("task-ongoing")).pixmap(iconSize);
                case QWebEngineDownloadItem::DownloadInProgress:
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

    return CDefaults::downloadManagerColumnCount;
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
        const QUuid id = rpl->property(CDefaults::replyAuxId).toUuid();
        row = m_downloads.indexOf(CDownloadItem(id));
    } else {
        qCritical() << "Download finished fast cleanup, unable to track download";
        return;
    }
    Q_ASSERT((row>=0) && (row<m_downloads.count()));

    if (item) {
        m_downloads[row].state = item->state();
        m_downloads[row].downloadItem = nullptr;

    } else if (rpl) {
        bool success = true;
        int status = CGenericFuncs::getHttpStatusFromReply(rpl.data());
        if ((rpl->error()!=QNetworkReply::NoError) || (status>=CDefaults::httpCodeRedirect)) {
            success = false;
            m_downloads[row].state = QWebEngineDownloadItem::DownloadInterrupted;
            m_downloads[row].errorString = tr("Error %1: %2").arg(rpl->error()).arg(rpl->errorString());
        }
        m_downloads[row].reply = nullptr;

        QPointer<CDownloadWriter> writer = m_downloads.at(row).writer;
        if (writer) {
            QMetaObject::invokeMethod(writer,[writer,success](){
                writer->finalizeFile(success);
            },Qt::QueuedConnection);
        }
    }

    Q_EMIT dataChanged(index(row,0),index(row,CDefaults::downloadManagerColumnCount-1)); // NOLINT

    updateProgressLabel();

    if ((m_downloads.at(row).autoDelete) ||
            (item && (item->state() == QWebEngineDownloadItem::DownloadCompleted)
             && gSet->settings()->downloaderCleanCompleted)) {

        deleteDownloadItem(index(row,0));
    }
}

void CDownloadsModel::makeWriterJob(CDownloadItem &item) const
{
    item.writer = new CDownloadWriter(nullptr,
                                      item.getZipName(),
                                      item.getFileName(),
                                      item.initialOffset,
                                      item.auxId);
    if (!gSet->startup()->setupThreadedWorker(item.writer)) {
        delete item.writer.data();
        return;
    }

    connect(item.writer,&CDownloadWriter::writeComplete,
            this,&CDownloadsModel::writerCompleted,Qt::QueuedConnection);
    connect(item.writer,&CDownloadWriter::error,
            this,&CDownloadsModel::writerError,Qt::QueuedConnection);

    QMetaObject::invokeMethod(item.writer,&CAbstractThreadWorker::start,Qt::QueuedConnection);
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

    Q_EMIT dataChanged(index(row,0),index(row,CDefaults::downloadManagerColumnCount-1));
}

void CDownloadsModel::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    auto *item = qobject_cast<QWebEngineDownloadItem *>(sender());
    auto *rpl = qobject_cast<QNetworkReply *>(sender());

    int row = -1;

    if (item) {
        row = m_downloads.indexOf(CDownloadItem(item->id()));
    } else if (rpl) {
        const QUuid id = rpl->property(CDefaults::replyAuxId).toUuid();
        row = m_downloads.indexOf(CDownloadItem(id));
    } else {
        qCritical() << "Download fast cleanup, unable to track download";
        return;
    }
    Q_ASSERT((row>=0) && (row<m_downloads.count()));

    m_downloads[row].oldReceived = m_downloads.at(row).received;
    m_downloads[row].received = bytesReceived + m_downloads.at(row).initialOffset;
    m_downloads[row].total = bytesTotal + m_downloads.at(row).initialOffset;

    m_manager->addReceivedBytes(m_downloads.at(row).received
                                - m_downloads.at(row).oldReceived);

    if ((rpl != nullptr) && (m_downloads.at(row).state == QWebEngineDownloadItem::DownloadRequested)) {
        m_downloads[row].state = QWebEngineDownloadItem::DownloadInProgress;
        Q_EMIT dataChanged(index(row,0),index(row,CDefaults::downloadManagerColumnCount-1));
    }

    updateProgressLabel();

    Q_EMIT dataChanged(index(row,2),index(row,CDefaults::downloadManagerColumnCount-1));
}

void CDownloadsModel::abortDownload()
{
    auto *acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=m_downloads.count()) return;

    if (!abortDownloadPriv(row)) {
        QMessageBox::warning(m_manager,QGuiApplication::applicationDisplayName(),
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
        if ((m_downloads.at(row).state!=QWebEngineDownloadItem::DownloadInProgress) &&
                (m_downloads.at(row).state!=QWebEngineDownloadItem::DownloadRequested)) {
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

    if (!QProcess::startDetached(QSL("xdg-open"), QStringList() << fi.path())) {
        QMessageBox::critical(m_manager, QGuiApplication::applicationDisplayName(),
                              tr("Unable to start browser."));
    }
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
        new CBrowserTab(gSet->activeWindow(),QUrl::fromLocalFile(fname));
}

void CDownloadsModel::openXdg()
{
    auto *acm = qobject_cast<QAction *>(sender());
    if (acm==nullptr) return;

    int row = acm->data().toInt();
    if (row<0 || row>=m_downloads.count()) return;
    if (!m_downloads.at(row).getZipName().isEmpty()) return;

    if (!QProcess::startDetached(QSL("xdg-open"), QStringList() << m_downloads.at(row).getFileName())) {
        QMessageBox::critical(m_manager, QGuiApplication::applicationDisplayName(),
                              tr("Unable to open application."));
    }
}

void CDownloadsModel::writerError(const QString &message)
{
    QPointer<CDownloadWriter> writer(qobject_cast<CDownloadWriter *>(sender()));
    if (writer.isNull()) {
        qCritical() << "Download writer fast cleanup, unable to track download, received error: "
                    << message;
        return;
    }

    int row = m_downloads.indexOf(CDownloadItem(writer->getAuxId()));
    if (row<0 || row>=m_downloads.count())
        return;

    m_downloads[row].state = QWebEngineDownloadItem::DownloadCancelled;
    m_downloads[row].errorString = message;
    updateProgressLabel();

    Q_EMIT dataChanged(index(row,0),index(row,CDefaults::downloadManagerColumnCount-1));
}

void CDownloadsModel::writerCompleted(bool success)
{
    QPointer<CDownloadWriter> writer(qobject_cast<CDownloadWriter *>(sender()));
    if (writer.isNull()) {
        qCritical() << "Download writer fast cleanup, unable to track download";
        return;
    }

    int row = m_downloads.indexOf(CDownloadItem(writer->getAuxId()));
    if (row<0 || row>=m_downloads.count())
        return;

    if (success) {
        m_downloads[row].state = QWebEngineDownloadItem::DownloadCompleted;
    } else {
        m_downloads[row].state = QWebEngineDownloadItem::DownloadInterrupted;
    }
    updateProgressLabel();

    Q_EMIT dataChanged(index(row,0),index(row,CDefaults::downloadManagerColumnCount-1));

    if (gSet->settings()->downloaderCleanCompleted && success)
        deleteDownloadItem(index(row,0));
}

CDownloadItem::CDownloadItem(quint32 itemId)
{
    id = itemId;
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

CDownloadItem::CDownloadItem(QNetworkReply *rpl, const QString &fname, const qint64 offset)
{
    pathName = fname;
    mimeType = QSL("application/download");
    reply = rpl;
    url = rpl->url();
    initialOffset = offset;
    received = offset;
    auxId = QUuid::createUuid();

    rpl->setProperty(CDefaults::replyAuxId,auxId);
}

bool CDownloadItem::operator==(const CDownloadItem &s) const
{
    if (id!=0 && s.id!=0)
        return (id==s.id);

    return auxId==s.auxId;
}

bool CDownloadItem::operator!=(const CDownloadItem &s) const
{
    return !operator==(s);
}

bool CDownloadItem::isEmpty() const
{
    return (id==0 && reply==nullptr && writer.isNull() && downloadItem==nullptr && pathName.isEmpty());
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
