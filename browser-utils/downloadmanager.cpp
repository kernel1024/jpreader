#include <algorithm>

#include <QWidget>
#include <QCloseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QDebug>

#include "downloadmanager.h"
#include "downloadwriter.h"
#include "downloadmodel.h"
#include "downloadlistmodel.h"
#include "extractors/fanboxextractor.h"
#include "extractors/patreonextractor.h"
#include "extractors/pixivnovelextractor.h"
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/ui.h"
#include "global/network.h"
#include "global/browserfuncs.h"
#include "global/startup.h"
#include "browser/browser.h"
#include "manga/mangaviewtab.h"
#include "ui_downloadmanager.h"
#include "ui_downloadlistdlg.h"

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

    connect(ui->buttonAbortActive, &QPushButton::clicked,
            m_model, &CDownloadsModel::abortActive);

    connect(ui->buttonAbortAll, &QPushButton::clicked,
            m_model, &CDownloadsModel::abortAll);

    connect(ui->spinLimitCount, &QSpinBox::valueChanged,[](int value){
        gSet->browser()->setDownloadsLimit(value);
    });

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

    bool isKemono = url.host().endsWith(QSL("kemono.party"),Qt::CaseInsensitive);

    // create common request (for HEAD and GET)
    QNetworkRequest req(url);
    req.setRawHeader("referer",referer.toString().toUtf8());
    if (isFanbox)
        req.setRawHeader("origin","https://www.fanbox.cc");
    if (isPatreon || isKemono) {
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::UserVerifiedRedirectPolicy);
    } else {
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);
    }
    req.setMaximumRedirectsAllowed(CDefaults::httpMaxRedirects);

    // we need HEAD request for file size calculation
    if (offset > 0L) {
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerHead(req,true);
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

bool CDownloadsModel::createDownloadForNetworkRequest(const QNetworkRequest &request, const QString &fileName,
                                                     qint64 offset, const QUuid &reuseExistingDownloadItem)
{
    if ((gSet->browser()->downloadsLimit()>0) && reuseExistingDownloadItem.isNull()) {
        if (m_downloads.count() > gSet->browser()->downloadsLimit()) {
            QMutexLocker lock(&m_tasksMutex);
            m_tasks.enqueue(CDownloadTask(request,fileName,offset));
            return false;
        }
    }

    int row = -1;
    if (!reuseExistingDownloadItem.isNull()) {
        row = m_downloads.indexOf(CDownloadItem(reuseExistingDownloadItem));
        if (row<0 || row>=m_downloads.count()) {
            qWarning() << "Download removed by user, abort restarting process.";
            return false;
        }
    }

    QNetworkRequest req = request;
    req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute,true);
    QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req,true);

    if (row<0) {
        appendItem(CDownloadItem(rpl, fileName, offset));
    } else {
        m_downloads[row].reuseReply(rpl);
    }

    connect(rpl, &QNetworkReply::finished,
            this, &CDownloadsModel::downloadFinished);
    connect(rpl, &QNetworkReply::downloadProgress,this,[this,rpl](qint64 bytesReceived, qint64 bytesTotal){
        auxDownloadProgress(bytesReceived,bytesTotal,rpl);
    });

    if (rpl->request().attribute(QNetworkRequest::RedirectPolicyAttribute).toInt()
            == QNetworkRequest::UserVerifiedRedirectPolicy) {
        connect(rpl,&QNetworkReply::redirected,this,&CDownloadsModel::requestRedirected);
    }

    connect(rpl, &QNetworkReply::readyRead, this, [this,rpl](){

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

    return true;
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
    if ((url.host().endsWith(QSL("patreon.com"),Qt::CaseInsensitive) &&
         (rpl->request().url().host().endsWith(QSL(".patreon.com"),Qt::CaseInsensitive) ||
          rpl->request().url().host().endsWith(QSL(".patreonusercontent.com"))))
            ||
            (url.host().endsWith(QSL("kemono.party"),Qt::CaseInsensitive) &&
             rpl->request().url().host().endsWith(QSL("kemono.party")))) {

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

void CDownloadManager::handleDownload(QWebEngineDownloadRequest *item)
{
    if (item==nullptr) return;

    if (!isVisible())
        show();

    if (item->savePageFormat()==CDownloadPageFormat::UnknownSaveFormat) {
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
    connect(item, &QWebEngineDownloadRequest::totalBytesChanged,m_model,[this,item](){
        m_model->auxDownloadProgress(item->receivedBytes(),item->totalBytes(),item);
    });
    connect(item, &QWebEngineDownloadRequest::receivedBytesChanged,m_model,[this,item](){
        m_model->auxDownloadProgress(item->receivedBytes(),item->totalBytes(),item);
    });
    connect(item, &QWebEngineDownloadRequest::stateChanged,
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

    if (item.state==CDownloadState::DownloadInProgress) {
        acm = cm.addAction(tr("Abort"),m_model,&CDownloadsModel::abortDownload);
        acm->setData(idx.row());
        cm.addSeparator();
    }

    acm = cm.addAction(tr("Remove"),m_model,&CDownloadsModel::cleanDownload);
    acm->setData(idx.row());

    if (item.state==CDownloadState::DownloadCompleted ||
            item.state==CDownloadState::DownloadInProgress) {
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

    ui->spinLimitCount->blockSignals(true);
    ui->spinLimitCount->setValue(gSet->browser()->downloadsLimit());
    ui->spinLimitCount->blockSignals(false);

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

void CDownloadManager::multiFileDownload(const QVector<CUrlWithName> &urls, const QUrl& referer,
                                         const QString& containerName, bool isFanbox, bool isPatreon)
{
    static QSize multiImgDialogSize = QSize();

    QDialog dlg(gSet->activeWindow());
    Ui::DownloadListDlg ui;
    ui.setupUi(&dlg);
    dlg.setWindowTitle(tr("Multiple files download"));

    auto *model = new CDownloadListModel(&dlg);
    int row = 0;
    int rejected = 0;
    for (const auto& url : urls) {
        const QUrl u(url.first);
        if (!u.isValid() || u.isRelative()) {
            rejected++;
            continue;
        }

        QString s = url.second;
        if (s.isEmpty())
            s = CGenericFuncs::decodeHtmlEntities(u.fileName());
        model->appendItem(s,url.first);
        row++;
    }
    ui.table->resizeColumnsToContents();

    auto *proxy = new QSortFilterProxyModel(&dlg);
    proxy->setSourceModel(model);
    ui.table->setModel(proxy);
    proxy->setFilterKeyColumn(-1);

    if (multiImgDialogSize.isValid())
        dlg.resize(multiImgDialogSize);

    connect(ui.filter,&QLineEdit::textEdited,&dlg,[ui,proxy](const QString& text){
        if (ui.syntax->currentIndex() == 0) {
            proxy->setFilterRegularExpression(text);
        } else {
            proxy->setFilterWildcard(text);
        }
    });
    connect(ui.syntax,&QComboBox::currentIndexChanged,&dlg,[ui,proxy](int index){
        const QString text = ui.filter->text();
        if (index == 0) {
            proxy->setFilterRegularExpression(text);
        } else {
            proxy->setFilterWildcard(text);
        }
    });

    auto updateLabelText = [ui,urls,rejected](const QItemSelection &selected = QItemSelection(),
                           const QItemSelection &deselected = QItemSelection()){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        QString res;
        int selectedCount = ui.table->selectionModel()->selectedRows(0).count();
        res = tr("%1 downloadable URLs detected.").arg(urls.count());
        if (rejected > 0)
            res.append(tr(" %1 rejected.").arg(rejected));
        if (selectedCount > 0)
            res.append(tr(" %1 selected.").arg(selectedCount));
        ui.label->setText(res);
    };

    ui.table->resizeColumnsToContents();
    updateLabelText();
    connect(ui.table->selectionModel(),&QItemSelectionModel::selectionChanged,updateLabelText);

    ui.syntax->setCurrentIndex(0);
    ui.checkAddNumbers->setChecked(isFanbox || isPatreon);

    if (!containerName.isEmpty())
        ui.table->selectAll();

    if (dlg.exec()==QDialog::Rejected) return;

    multiImgDialogSize=dlg.size();
    const QModelIndexList selectedItems = ui.table->selectionModel()->selectedRows(0);

    bool onlyFanboxPosts = std::all_of(selectedItems.constBegin(),selectedItems.constEnd(),[model,proxy](const QModelIndex& item){
        static const QRegularExpression rxFanboxPostId(QSL("fanbox.cc/.*posts/(?<postID>\\d+)"));
        const QString url = model->getUrl(proxy->mapToSource(item));
        auto match = rxFanboxPostId.match(url);
        return match.hasMatch();
    });
    bool onlyPixivNovels = std::all_of(selectedItems.constBegin(),selectedItems.constEnd(),[model,proxy](const QModelIndex& item){
        const QString url = model->getUrl(proxy->mapToSource(item));
        return url.contains(QSL("pixiv.net/novel/show.php"), Qt::CaseInsensitive);
    });

    QString container;
    if (ui.checkZipFile->isChecked() && !(onlyFanboxPosts || onlyPixivNovels)) {
        QString fname = containerName;
        if (!fname.endsWith(QSL(".zip"),Qt::CaseInsensitive))
            fname += QSL(".zip");
        container = CGenericFuncs::getSaveFileNameD(gSet->activeWindow(),
                                                    tr("Pack files to ZIP file"),CGenericFuncs::getTmpDir(),
                                                    QStringList( { tr("ZIP file (*.zip)") } ),nullptr,fname);
    } else {
        container = CGenericFuncs::getExistingDirectoryD(gSet->activeWindow(),tr("Save to directory"),
                                                         CGenericFuncs::getTmpDir(),
                                                         QFileDialog::ShowDirsOnly,containerName);
    }
    if (container.isEmpty()) return;

    int index = 0;
    if (onlyFanboxPosts || onlyPixivNovels) {
        QStringList urls;
        urls.reserve(selectedItems.count());
        for (const auto &item : selectedItems) {
            const QString url = model->getUrl(proxy->mapToSource(item));
            if (!urls.contains(url))
                urls.append(url);
        }

        for (const auto &url : qAsConst(urls)) {
            if (!ui.checkAddNumbers->isChecked()) {
                index = -1;
            } else {
                index++;
            }

            CAbstractExtractor *ex = nullptr;
            if (onlyFanboxPosts) {
                int id = -1;
                static const QRegularExpression rxFanboxPostId(QSL("fanbox.cc/.*posts/(?<postID>\\d+)"));
                auto mchFanboxPostId = rxFanboxPostId.match(url);
                if (mchFanboxPostId.hasMatch()) {
                    bool ok = false;
                    id = mchFanboxPostId.captured(QSL("postID")).toInt(&ok);
                    if (ok) {
                        ex = new CFanboxExtractor(nullptr);
                        qobject_cast<CFanboxExtractor *>(ex)->setParams(id,false,false,false,false,true,
                                                                        { { QSL("containerPath"), container },
                                                                          { QSL("index"), QSL("%1").arg(index) },
                                                                          { QSL("count"), QSL("%1").arg(urls.count()) } });
                    }
                }

            } else if (onlyPixivNovels) {
                ex = new CPixivNovelExtractor(nullptr);
                qobject_cast<CPixivNovelExtractor *>(ex)->setParams(url,QString(),false,false,false,true,
                                                                    { { QSL("containerPath"), container },
                                                                      { QSL("index"), QSL("%1").arg(index) },
                                                                      { QSL("count"), QSL("%1").arg(urls.count()) } });
            }
            if (!gSet->startup()->setupThreadedWorker(ex)) {
                delete ex;
            } else {
                QMetaObject::invokeMethod(ex,&CAbstractThreadWorker::start,Qt::QueuedConnection);
            }
        }

        return;
    }

    bool forceOverwrite = false;
    for (const auto &item : selectedItems){
        if (!ui.checkAddNumbers->isChecked()) {
            index = -1;
        } else {
            index++;
        }

        const QString filename = model->getFileName(proxy->mapToSource(item));
        const QString url = model->getUrl(proxy->mapToSource(item));

        handleAuxDownload(url,filename,container,referer,index,
                          selectedItems.count(),isFanbox,isPatreon,forceOverwrite);
    }
}

void CDownloadManager::novelReady(const QString &html, bool focus, bool translate, bool alternateTranslate,
                                  bool downloadNovel, const CStringHash &info)
{
    if (downloadNovel) {
        bool zipTarget = false;
        QString fname;
        const QString containerPath = info.value(QSL("containerPath"));
        bool ok = false;
        int index = info.value(QSL("index"),QSL("-1")).toInt(&ok);
        if (!ok) index = -1;
        int count = info.value(QSL("count"),QSL("-1")).toInt(&ok);
        if (!ok) count = -1;
        QString title = info.value(QSL("title"));
        if (!info.value(QSL("suggestedTitle")).isEmpty())
            title = info.value(QSL("suggestedTitle"));

        const QString suggestedFileName = QSL("%1 - %2.htm").arg(info.value(QSL("id")),
                                                                 CGenericFuncs::makeSafeFilename(title));

        if (!computeFileName(fname,
                             zipTarget,
                             index,
                             count,
                             QUrl(),
                             containerPath,
                             suggestedFileName)) {

            QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                                  tr("Computed file name is empty."));
            return;
        }

        gSet->ui()->setSavedAuxSaveDir(containerPath);

        QFile file(fname);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                                  tr("Unable to create file:\n%1.").arg(fname));
            return;
        }
        file.write(html.toUtf8());
        file.close();

        return;
    }

    new CBrowserTab(gSet->activeWindow(),QUrl(),QStringList(),html,focus,false,
                    translate,alternateTranslate);
}

void CDownloadManager::mangaReady(const QVector<CUrlWithName> &urls, const QString &containerName, const QUrl &origin,
                                  const QString &title, const QString &description,
                                  bool useViewer, bool focus, bool originalScale)
{
    bool isFanbox = (qobject_cast<CFanboxExtractor *>(sender()) != nullptr);
    bool isPatreon = (qobject_cast<CPatreonExtractor *>(sender()) != nullptr);

    if (urls.isEmpty() || containerName.isEmpty()) {
        QMessageBox::warning(gSet->activeWindow(),QGuiApplication::applicationDisplayName(),
                             tr("Image urls not found or container name not detected."));
    } else if (useViewer) {
        auto *mv = new CMangaViewTab(gSet->activeWindow(),focus);
        QString mangaTitle = containerName;
        if (!title.isEmpty())
            mangaTitle = title;
        mv->loadMangaPages(urls,mangaTitle,description,origin,isFanbox,originalScale);
    } else {
        multiFileDownload(urls,origin,containerName,isFanbox,isPatreon);
    }
}

CDownloadBarDelegate::CDownloadBarDelegate(QObject *parent, CDownloadsModel *model)
    : QStyledItemDelegate(parent),
      m_model(model)
{
}

void CDownloadBarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() == 2) {
        int progress = index.data(Qt::UserRole+1).toInt();
        const int topMargin = 10;
        const int bottomMargin = 5;

        QStyleOptionProgressBar progressBarOption;
        QRect r = option.rect;
        r.setHeight(r.height()-topMargin); r.moveTop(r.top()+bottomMargin);
        progressBarOption.rect = r;
        progressBarOption.invertedAppearance = false;
        progressBarOption.bottomToTop = false;
        progressBarOption.palette = option.palette;
        progressBarOption.state = QStyle::State_Horizontal;
        progressBarOption.textAlignment = Qt::AlignCenter;
        progressBarOption.minimum = 0;
        if (progress>=0) {
            progressBarOption.maximum = 100;
            progressBarOption.progress = progress;
            progressBarOption.text = QSL("%1%").arg(progress);
            progressBarOption.textVisible = true;
        } else {
            progressBarOption.maximum = 0;
            progressBarOption.progress = 0;
            progressBarOption.textVisible = false;
        }
        QApplication::style()->drawControl(QStyle::CE_ProgressBar,
                                           &progressBarOption, painter);
    } else {
        QStyledItemDelegate::paint(painter,option,index);
    }
}

QSize CDownloadBarDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)

    const QSize defaultSize(200,32);
    return defaultSize;
}
