#include <QIcon>
#include <QProcess>
#include <QMessageBox>

#include "downloadmanager.h"
#include "downloadmodel.h"
#include "global/structures.h"
#include "global/control.h"
#include "global/startup.h"
#include "utils/genericfuncs.h"
#include "browser/browser.h"

CDownloadsModel::CDownloadsModel(CDownloadManager *parent)
    : QAbstractTableModel(parent),
      m_manager(parent)
{
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
                case CDownloadState::DownloadCancelled:
                    return QIcon::fromTheme(QSL("task-reject")).pixmap(iconSize);
                case CDownloadState::DownloadCompleted:
                    return QIcon::fromTheme(QSL("task-complete")).pixmap(iconSize);
                case CDownloadState::DownloadRequested:
                    return QIcon::fromTheme(QSL("task-ongoing")).pixmap(iconSize);
                case CDownloadState::DownloadInProgress:
                    return QIcon::fromTheme(QSL("arrow-right")).pixmap(iconSize);
                case CDownloadState::DownloadInterrupted:
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
                if (t.total<=0) return QString();
                return QSL("%1%").arg(100*t.received/t.total);
            case 3:
                if (!t.errorString.isEmpty())
                    return t.errorString;
                return QSL("%1 / %2")
                        .arg(CGenericFuncs::formatFileSize(t.received),
                             CGenericFuncs::formatFileSize(t.total));
            default: return QVariant();
        }

    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == 2)
            return Qt::AlignCenter;

    } else if (role == Qt::ToolTipRole || role == Qt::StatusTipRole) {
        if (!t.errorString.isEmpty() && index.column()==0)
            return t.errorString;
        if (!zfname.isEmpty())
            return tr("%1\nZip: %2)").arg(t.getFileName(),zfname);
        return t.getFileName();

    } else if (role == Qt::UserRole+1) {
        if (t.total<=0) return 0;
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
    downloadFinishedPrev(sender());
}

void CDownloadsModel::downloadFinishedPrev(QObject* source)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    QScopedPointer<QWebEngineDownloadItem,QScopedPointerDeleteLater>
            item(qobject_cast<QWebEngineDownloadItem *>(source));
#else
    QScopedPointer<QWebEngineDownloadRequest,QScopedPointerDeleteLater>
            item(qobject_cast<QWebEngineDownloadRequest *>(source));
#endif
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(source));

    int row = -1;
    QUuid auxId;
    if (item) {
        row = m_downloads.indexOf(CDownloadItem(item->id()));
    } else if (rpl) {
        auxId = rpl->property(CDefaults::replyAuxId).toUuid();
        row = m_downloads.indexOf(CDownloadItem(auxId));
    } else {
        qCritical() << "Download finished fast cleanup, unable to track download";
        return;
    }
    Q_ASSERT((row>=0) && (row<m_downloads.count()));

    bool isRestarting = false;
    if (item) {
        m_downloads[row].state = item->state();
        m_downloads[row].downloadItem = nullptr;

    } else if (rpl) {
        bool success = true;
        int status = CGenericFuncs::getHttpStatusFromReply(rpl.data());
        if ((rpl->error()!=QNetworkReply::NoError) || (status>=CDefaults::httpCodeRedirect)) {
            success = false;
            m_downloads[row].state = CDownloadState::DownloadInterrupted;
            m_downloads[row].errorString = tr("Error %1: %2").arg(rpl->error()).arg(rpl->errorString());
            if (!m_downloads.at(row).aborted &&
                    (m_downloads.at(row).retries < gSet->settings()->translatorRetryCount)) {
                m_downloads[row].retries++;
                m_downloads[row].errorString.append(tr(" Restarting %1 of %2.")
                                                    .arg(m_downloads.at(row).retries)
                                                    .arg(gSet->settings()->translatorRetryCount));
                isRestarting = true;
            }
        }
        m_downloads[row].reply = nullptr;

        QPointer<CDownloadWriter> writer = m_downloads.at(row).writer;
        if (writer) {
            QMetaObject::invokeMethod(writer,[writer,success,isRestarting](){
                writer->finalizeFile(success,isRestarting);
            },Qt::QueuedConnection);
        }

        if (isRestarting) {
            const QNetworkRequest req = rpl->request();
            const int retries = m_downloads.at(row).retries;
            QTimer::singleShot(CDefaults::retryRestartMS,this,[this,req,auxId,retries](){
                qWarning() << QSL("Restarting download %1 of %2 for %3")
                              .arg(retries)
                              .arg(gSet->settings()->translatorRetryCount)
                              .arg(req.url().toString());
                createDownloadForNetworkRequest(req,QString(),0U,auxId);
            });
        }
    }

    Q_EMIT dataChanged(index(row,0),index(row,CDefaults::downloadManagerColumnCount-1)); // NOLINT

    updateProgressLabel();

    if (!isRestarting && ((m_downloads.at(row).autoDelete) ||
            (item && (item->state() == CDownloadState::DownloadCompleted)
             && gSet->settings()->downloaderCleanCompleted))) {

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

void CDownloadsModel::downloadStateChanged(CDownloadState state)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    auto *item = qobject_cast<QWebEngineDownloadItem *>(sender());
#else
    auto *item = qobject_cast<QWebEngineDownloadRequest *>(sender());
#endif
    if (item==nullptr) return;

    int row = m_downloads.indexOf(CDownloadItem(item->id()));
    if (row<0 || row>=m_downloads.count()) return;

    m_downloads[row].state = state;
    if (item->interruptReason() != CDownloadInterruptReason::NoReason)
        m_downloads[row].errorString = item->interruptReasonString();

    updateProgressLabel();

    Q_EMIT dataChanged(index(row,0),index(row,CDefaults::downloadManagerColumnCount-1));

    if (state == CDownloadState::DownloadCompleted)
        downloadFinishedPrev(item);
}

void CDownloadsModel::auxDownloadProgress(qint64 bytesReceived, qint64 bytesTotal, QObject* source)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    auto *item = qobject_cast<QWebEngineDownloadItem *>(source);
#else
    auto *item = qobject_cast<QWebEngineDownloadRequest *>(source);
#endif
    auto *rpl = qobject_cast<QNetworkReply *>(source);

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

    if ((rpl != nullptr) && (m_downloads.at(row).state == CDownloadState::DownloadRequested)) {
        m_downloads[row].state = CDownloadState::DownloadInProgress;
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
    if ((m_downloads.at(row).state==CDownloadState::DownloadInProgress) ||
            (m_downloads.at(row).state==CDownloadState::DownloadRequested)) {
        if (m_downloads.at(row).downloadItem) {
            m_downloads[row].downloadItem->cancel();
            m_downloads[row].aborted = true;
            res = true;
        } else if (m_downloads.at(row).reply) {
            m_downloads[row].reply->abort();
            m_downloads[row].aborted = true;
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
    if (m_downloads.at(row).state==CDownloadState::DownloadInProgress) {
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
        if ((m_downloads.at(row).state!=CDownloadState::DownloadInProgress) &&
                (m_downloads.at(row).state!=CDownloadState::DownloadRequested)) {
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
        if (m_downloads.at(row).state==CDownloadState::DownloadCompleted) {
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

    m_downloads[row].state = CDownloadState::DownloadCancelled;
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
        m_downloads[row].state = CDownloadState::DownloadCompleted;
    } else {
        m_downloads[row].state = CDownloadState::DownloadInterrupted;
    }
    updateProgressLabel();

    Q_EMIT dataChanged(index(row,0),index(row,CDefaults::downloadManagerColumnCount-1));

    if (gSet->settings()->downloaderCleanCompleted && success)
        deleteDownloadItem(index(row,0));
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
    int retries = 0;
    for(const auto &item : qAsConst(m_downloads)) {
        retries += item.retries;
        switch (item.state) {
            case CDownloadState::DownloadCancelled:
                cancelled++;
                break;
            case CDownloadState::DownloadCompleted:
                completed++;
                break;
            case CDownloadState::DownloadInProgress:
            case CDownloadState::DownloadRequested:
                active++;
                break;
            case CDownloadState::DownloadInterrupted:
                failures++;
                break;
        }
    }

    m_manager->setProgressLabel(tr("Downloaded: %1 of %2. Active: %3. Failures: %4. Cancelled %5.\n"
                                   "Total downloaded: %6. Retries: %7.")
                                .arg(completed)
                                .arg(total)
                                .arg(active)
                                .arg(failures)
                                .arg(cancelled)
                                .arg(CGenericFuncs::formatFileSize(m_manager->receivedBytes()))
                                .arg(retries));
}

CDownloadItem::CDownloadItem(quint32 itemId)
{
    id = itemId;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
CDownloadItem::CDownloadItem(QWebEngineDownloadItem* item)
#else
CDownloadItem::CDownloadItem(QWebEngineDownloadRequest* item)
#endif
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

void CDownloadItem::reuseReply(QNetworkReply *rpl)
{
    reply = rpl;
    url = rpl->url();
    received = initialOffset;

    errorString.clear();
    state = CDownloadState::DownloadRequested;
    oldReceived = 0L;
    total = 0L;

    rpl->setProperty(CDefaults::replyAuxId,auxId);
}
