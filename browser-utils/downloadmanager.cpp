#include <QWidget>
#include <QCloseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QDebug>

#include "downloadmanager.h"
#include "downloadwriter.h"
#include "downloadmodel.h"
#include "extractors/fanboxextractor.h"
#include "extractors/patreonextractor.h"
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/ui.h"
#include "global/network.h"
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
    if (!CDownloadManager::computeFileName(fname,isZipTarget,index,maxIndex,url,containerPath,suggestedFilename)) {
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

void CDownloadsModel::createDownloadForNetworkRequest(const QNetworkRequest &request, const QString &fileName,
                                                     qint64 offset, const QUuid &reuseExistingDownloadItem)
{
    int row = -1;
    if (!reuseExistingDownloadItem.isNull()) {
        row = m_downloads.indexOf(CDownloadItem(reuseExistingDownloadItem));
        if (row<0 || row>=m_downloads.count()) {
            qWarning() << "Download removed by user, abort restarting process.";
            return;
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

#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
void CDownloadManager::handleDownload(QWebEngineDownloadItem *item)
#else
void CDownloadManager::handleDownload(QWebEngineDownloadRequest *item)
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    connect(item, &QWebEngineDownloadItem::finished,
            m_model, &CDownloadsModel::downloadFinished);
    connect(item, &QWebEngineDownloadItem::downloadProgress,this,
            [this,item](qint64 bytesReceived, qint64 bytesTotal){
        m_model->auxDownloadProgress(bytesReceived,bytesTotal,item);
    });
    connect(item, &QWebEngineDownloadItem::stateChanged,
            m_model, &CDownloadsModel::downloadStateChanged);
#else
    connect(item, &QWebEngineDownloadRequest::totalBytesChanged,m_model,[this,item](){
        m_model->auxDownloadProgress(item->receivedBytes(),item->totalBytes(),item);
    });
    connect(item, &QWebEngineDownloadRequest::receivedBytesChanged,m_model,[this,item](){
        m_model->auxDownloadProgress(item->receivedBytes(),item->totalBytes(),item);
    });
    connect(item, &QWebEngineDownloadRequest::stateChanged,
            m_model, &CDownloadsModel::downloadStateChanged);
#endif

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
    if (multiImgDialogSize.isValid())
        dlg.resize(multiImgDialogSize);

    connect(ui.filter,&QLineEdit::textEdited,[ui](const QString& text){
        if (text.isEmpty()) {
            for (int i=0;i<ui.table->rowCount();i++) {
                if (ui.table->isRowHidden(i))
                    ui.table->showRow(i);
            }
            return;
        }

        for (int i=0;i<ui.table->rowCount();i++) {
            ui.table->hideRow(i);
        }
        const QList<QTableWidgetItem *> filtered
                = ui.table->findItems(text,
                                      ((ui.syntax->currentIndex()==0) ?
                                           Qt::MatchRegularExpression : Qt::MatchWildcard));
        for (auto *item : filtered) {
            ui.table->showRow(item->row());
        }
    });

    ui.label->setText(tr("%1 downloadable URLs detected.").arg(urls.count()));
    ui.syntax->setCurrentIndex(0);
    ui.checkAddNumbers->setChecked(isFanbox || isPatreon);

    ui.table->setColumnCount(2);
    ui.table->setHorizontalHeaderLabels({ tr("File name"), tr("URL") });
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

        ui.table->setRowCount(row+1);
        ui.table->setItem(row,0,new QTableWidgetItem(s));
        ui.table->setItem(row,1,new QTableWidgetItem(url.first));

        row++;
    }
    ui.table->resizeColumnsToContents();

    if (rejected>0)
        ui.label->setText(QSL("%1. %2 incorrect URLs.").arg(ui.label->text()).arg(rejected));

    dlg.setWindowTitle(tr("Multiple images download"));
    if (!containerName.isEmpty())
        ui.table->selectAll();

    if (dlg.exec()==QDialog::Rejected) return;
    multiImgDialogSize=dlg.size();

    QString container;
    if (ui.checkZipFile->isChecked()) {
        QString fname = containerName;
        if (!fname.endsWith(QSL(".zip"),Qt::CaseInsensitive))
            fname += QSL(".zip");
        container = CGenericFuncs::getSaveFileNameD(gSet->activeWindow(),
                                                    tr("Pack images to ZIP file"),CGenericFuncs::getTmpDir(),
                                                    QStringList( { tr("ZIP file (*.zip)") } ),nullptr,fname);
    } else {
        container = CGenericFuncs::getExistingDirectoryD(gSet->activeWindow(),tr("Save images to directory"),
                                                         CGenericFuncs::getTmpDir(),
                                                         QFileDialog::ShowDirsOnly,containerName);
    }
    if (container.isEmpty()) return;

    int index = 0;
    const QModelIndexList itml = ui.table->selectionModel()->selectedRows(0);
    bool forceOverwrite = false;
    for (const auto &itm : itml){
        if (!ui.checkAddNumbers->isChecked()) {
            index = -1;
        } else {
            index++;
        }

        QString filename = ui.table->item(itm.row(),0)->text();
        QString url = ui.table->item(itm.row(),1)->text();

        handleAuxDownload(url,filename,container,referer,index,
                          itml.count(),isFanbox,isPatreon,forceOverwrite);
    }
}

void CDownloadManager::novelReady(const QString &html, bool focus, bool translate, bool alternateTranslate)
{
    new CBrowserTab(gSet->activeWindow(),QUrl(),QStringList(),html,focus,false,
                    translate,alternateTranslate);
}

void CDownloadManager::mangaReady(const QVector<CUrlWithName> &urls, const QString &containerName, const QUrl &origin,
                                  bool useViewer, bool focus, bool originalScale)
{
    bool isFanbox = (qobject_cast<CFanboxExtractor *>(sender()) != nullptr);
    bool isPatreon = (qobject_cast<CPatreonExtractor *>(sender()) != nullptr);

    if (urls.isEmpty() || containerName.isEmpty()) {
        QMessageBox::warning(gSet->activeWindow(),QGuiApplication::applicationDisplayName(),
                             tr("Image urls not found or container name not detected."));
    } else if (useViewer) {
        auto *mv = new CMangaViewTab(gSet->activeWindow(),focus);
        mv->loadMangaPages(urls,containerName,origin,isFanbox,originalScale);
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
