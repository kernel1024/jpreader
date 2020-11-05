#include <QTextCodec>
#include <QPointer>
#include <QMessageBox>
#include <QFileInfo>
#include <QWebEngineScriptCollection>
#include <QDirIterator>
#include <QDialog>
#include <QThread>
#include <QMimeData>
#include <QAuthenticator>

#include "snnet.h"
#include "snmsghandler.h"
#include "genericfuncs.h"
#include "authdlg.h"
#include "pdftotext.h"
#include "downloadmanager.h"
#include "genericfuncs.h"
#include "extractors/pixivnovelextractor.h"
#include "extractors/pixivindexextractor.h"
#include "extractors/fanboxextractor.h"
#include "extractors/patreonextractor.h"
#include "extractors/htmlimagesextractor.h"
#include "globalcontrol.h"
#include "ui_downloadlistdlg.h"

namespace CDefaults {
const int browserPageFinishedTimeoutMS = 30000;
const int clipboardHtmlAquireDelay = 1000;
}

CSnNet::CSnNet(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;

    m_finishedTimer.setInterval(CDefaults::browserPageFinishedTimeoutMS);
    m_finishedTimer.setSingleShot(true);
    connect(&m_finishedTimer,&QTimer::timeout,this,[this](){
        loadFinished(true);
    });
}

void CSnNet::multiImgDownload(const QVector<CUrlWithName> &urls, const QUrl& referer, const QString& containerName,
                              bool isFanbox, bool relaxedRedirects)
{
    static QSize multiImgDialogSize = QSize();

    if (gSet->downloadManager()==nullptr) return;

    QDialog dlg(snv);
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
        const QList<QTableWidgetItem *> filtered = ui.table->findItems(text,
                                                                       ((ui.syntax->currentIndex()==0) ?
                                                                            Qt::MatchRegularExpression : Qt::MatchWildcard));
        for (auto *item : filtered) {
            ui.table->showRow(item->row());
        }
    });

    ui.label->setText(tr("%1 image URLs detected.").arg(urls.count()));
    ui.syntax->setCurrentIndex(0);

    ui.table->setColumnCount(2);
    ui.table->setHorizontalHeaderLabels({ tr("File name"), tr("URL") });
    int row = 0;
    int rejected = 0;
    for (const auto& url : urls) {
        QUrl u(url.first);
        if (!u.isValid() || u.isRelative()) {
            rejected++;
            continue;
        }

        QString s = url.second;
        if (s.isEmpty())
            s = u.fileName();

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
        container = CGenericFuncs::getSaveFileNameD(snv,tr("Pack images to ZIP file"),CGenericFuncs::getTmpDir(),
                                                    QStringList( { tr("ZIP file (*.zip)") } ),nullptr,fname);
    } else {
        container = CGenericFuncs::getExistingDirectoryD(snv,tr("Save images to directory"),
                                                         CGenericFuncs::getTmpDir(),
                                                         QFileDialog::ShowDirsOnly,containerName);
    }
    if (container.isEmpty()) return;

    int index = 0;
    const QModelIndexList itml = ui.table->selectionModel()->selectedRows(0);
    for (const auto &itm : itml){
        if (!ui.checkAddNumbers->isChecked()) {
            index = -1;
        } else {
            index++;
        }

        QString filename = ui.table->item(itm.row(),0)->text();
        QString url = ui.table->item(itm.row(),1)->text();

        gSet->downloadManager()->handleAuxDownload(url,filename,container,referer,index,
                                                   itml.count(),isFanbox,relaxedRedirects);
    }
}

bool CSnNet::isValidLoadedUrl(const QUrl& url)
{
    // loadedUrl points to non-empty page
    if (!url.isValid()) return false;
    if (!url.toLocalFile().isEmpty()) return true;
    if (url.scheme().startsWith(QSL("http"),Qt::CaseInsensitive)) return true;
    return false;
}

bool CSnNet::isValidLoadedUrl()
{
    return isValidLoadedUrl(m_loadedUrl);
}

bool CSnNet::loadWithTempFile(const QString &html, bool createNewTab, bool autoTranslate)
{
    QByteArray ba = html.toUtf8();
    QString fname = gSet->makeTmpFileName(QSL("html"),true);
    QFile f(fname);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(ba);
        f.close();
        gSet->appendCreatedFiles(fname);
        if (createNewTab) {
            auto *sv = new CSnippetViewer(snv->parentWnd(),QUrl::fromLocalFile(fname));
            sv->m_requestAutotranslate = autoTranslate;
        } else {
            snv->m_fileChanged = false;
            snv->m_requestAutotranslate = autoTranslate;
            snv->txtBrowser->load(QUrl::fromLocalFile(fname));
            snv->m_auxContentLoaded=false;
        }
        return true;
    }

    QMessageBox::warning(snv,QGuiApplication::applicationDisplayName(),
                         tr("Unable to create temporary file for document."));
    return false;
}

void CSnNet::progressLoad(int progress)
{
    snv->barLoading->setValue(progress);
    m_finishedTimer.start();
}

void CSnNet::loadStarted()
{
    snv->barLoading->setValue(0);
    snv->barLoading->show();
    snv->barPlaceholder->hide();
    snv->m_loading = true;
    snv->updateButtonsState();
}

void CSnNet::loadFinished(bool ok)
{
    if (!snv->m_loading) return;

    snv->msgHandler->updateZoomFactor();
    snv->msgHandler->hideBarLoading();
    snv->m_loading = false;
    snv->updateButtonsState();

    if (isValidLoadedUrl(snv->txtBrowser->url()) && ok) {
        snv->m_auxContentLoaded=false;
        m_loadedUrl = snv->txtBrowser->url();
    }

    snv->updateTabColor(true,false);

    bool xorAutotranslate = (gSet->ui()->autoTranslate() && !snv->m_requestAutotranslate) ||
                            (!gSet->ui()->autoTranslate() && snv->m_requestAutotranslate);

    if (xorAutotranslate && !snv->m_onceTranslated && (isValidLoadedUrl() || snv->m_auxContentLoaded))
        snv->transButton->click();

    snv->m_pageLoaded = true;
    snv->m_requestAutotranslate = false;

    if (snv->tabWidget()->currentWidget()==snv) {
        snv->takeScreenshot();
        snv->txtBrowser->setFocus();
    }
}

void CSnNet::userNavigationRequest(const QUrl &url, int type, bool isMainFrame)
{
    Q_UNUSED(type)

    if (!isValidLoadedUrl(url)) return;

    snv->m_onceTranslated = false;
    snv->m_fileChanged = false;
    snv->m_auxContentLoaded=false;

    if (isMainFrame) {
        CUrlHolder uh(QSL("(blank)"),url);
        gSet->appendMainHistory(uh);
    }
}

void CSnNet::extractHTMLFragment()
{
    if (!snv->txtBrowser->hasSelection()) return;

    snv->txtBrowser->page()->action(QWebEnginePage::Copy)->activate(QAction::Trigger);
    const QUrl url = snv->txtBrowser->url();
    QTimer::singleShot(CDefaults::clipboardHtmlAquireDelay,this,[this,url](){
        QClipboard *cb = QApplication::clipboard();
        const QMimeData *md = cb->mimeData(QClipboard::Clipboard);
        if (md->hasHtml()) {
            auto *ex = new CHtmlImagesExtractor(nullptr,snv);
            auto *th = new QThread();
            ex->setParams(md->html(),url,false,true);

            connect(ex,&CHtmlImagesExtractor::novelReady,snv->netHandler,&CSnNet::novelReady,Qt::QueuedConnection);
            connect(ex,&CHtmlImagesExtractor::finished,th,&QThread::quit);
            connect(th,&QThread::finished,ex,&CHtmlImagesExtractor::deleteLater);
            connect(th,&QThread::finished,th,&QThread::deleteLater);
            connect(th,&QThread::finished,gSet,[](){
                gSet->app()->restoreOverrideCursor();
            },Qt::QueuedConnection);

            ex->moveToThread(th);
            th->start();

            gSet->app()->setOverrideCursor(Qt::BusyCursor);

            QMetaObject::invokeMethod(ex,&CHtmlImagesExtractor::start,Qt::QueuedConnection);
        } else {
            QMessageBox::information(snv,QGuiApplication::applicationDisplayName(),
                                     tr("Unknown clipboard format. HTML expected."));
        }
        cb->clear(QClipboard::Clipboard);
    });
}

void CSnNet::processExtractorAction()
{
    auto *ac = qobject_cast<QAction*>(sender());
    if (!ac) return;

    processExtractorActionIndirect(ac->data().toHash());
}

void CSnNet::processExtractorActionIndirect(const QVariantHash &params)
{
    auto *ex = CAbstractExtractor::extractorWorkerFactory(params,snv);
    if (ex == nullptr) return;

    connect(ex,&CAbstractExtractor::novelReady,this,&CSnNet::novelReady,Qt::QueuedConnection);
    connect(ex,&CAbstractExtractor::mangaReady,this,&CSnNet::mangaReady,Qt::QueuedConnection);

    QMetaObject::invokeMethod(ex,&CAbstractExtractor::start,Qt::QueuedConnection);
}

void CSnNet::novelReady(const QString &html, bool focus, bool translate)
{
    if (html.toUtf8().size()<CDefaults::maxDataUrlFileSize) {
        auto *sv = new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),focus,html);
        sv->m_requestAutotranslate = translate;
    } else {
        loadWithTempFile(html,true,translate);
    }
}

void CSnNet::mangaReady(const QVector<CUrlWithName> &urls, const QString &containerName, const QUrl &origin)
{
    bool isFanbox = (qobject_cast<CFanboxExtractor *>(sender()) != nullptr);
    bool isPatreon = (qobject_cast<CPatreonExtractor *>(sender()) != nullptr);

    if (urls.isEmpty() || containerName.isEmpty()) {
        QMessageBox::warning(snv,QGuiApplication::applicationDisplayName(),
                             tr("Image urls not found or container name not detected."));
    } else {
        multiImgDownload(urls,origin,containerName,isFanbox,isPatreon);
    }
}

void CSnNet::pdfConverted(const QString &html)
{
    if (html.isEmpty()) {
        snv->txtBrowser->setHtmlInterlocked(CGenericFuncs::makeSimpleHtml(QSL("PDF conversion error"),
                                                                          QSL("Empty document.")));
    }
    if (html.length()<CDefaults::maxDataUrlFileSize) { // Small PDF files
        snv->txtBrowser->setHtmlInterlocked(html);
        snv->m_auxContentLoaded=false;
    } else { // Big PDF files
        loadWithTempFile(html,false);
    }
}

void CSnNet::pdfError(const QString &message)
{
    QString cn=CGenericFuncs::makeSimpleHtml(tr("Error"),tr("Unable to open PDF file.<br>%1").arg(message));
    snv->txtBrowser->setHtmlInterlocked(cn);
    QMessageBox::critical(snv,QGuiApplication::applicationDisplayName(),
                          tr("Unable to open PDF file."));
}

void CSnNet::load(const QUrl &url)
{
    if (gSet->isUrlBlocked(url)) {
        qInfo() << "adblock - skipping" << url;
        return;
    }

    snv->updateWebViewAttributes();
    if (snv->m_startPage)
        snv->m_startPage = false;

    QString fname = url.toLocalFile();
    if (!fname.isEmpty()) {
        QFileInfo fi(fname);
        if (!fi.exists() || !fi.isReadable()) {
            QString cn=CGenericFuncs::makeSimpleHtml(tr("Error"),tr("Unable to find file '%1'.").arg(fname));
            snv->m_fileChanged = false;
            snv->m_translationBkgdFinished=false;
            snv->m_loadingBkgdFinished=false;
            snv->txtBrowser->setHtmlInterlocked(cn,url);
            QMessageBox::critical(snv,QGuiApplication::applicationDisplayName(),
                                  tr("Unable to open file."));
            return;
        }
        QString mime = CGenericFuncs::detectMIME(fname);
        if (mime.startsWith(QSL("text/plain"),Qt::CaseInsensitive) &&
                fi.size()<CDefaults::maxDataUrlFileSize) { // for small local txt files (load big files via file url directly)
            QFile data(fname);
            if (data.open(QFile::ReadOnly)) {
                QByteArray ba = data.readAll();
                QTextCodec* cd = CGenericFuncs::detectEncoding(ba);
                QString cn=CGenericFuncs::makeSimpleHtml(fi.fileName(),cd->toUnicode(ba));
                snv->m_fileChanged = false;
                snv->m_translationBkgdFinished=false;
                snv->m_loadingBkgdFinished=false;
                snv->txtBrowser->setHtmlInterlocked(cn,url);
                snv->m_auxContentLoaded=false;
                data.close();
            }
        } else if (mime.startsWith(QSL("application/pdf"),Qt::CaseInsensitive)) {
            // for local pdf files
            snv->m_fileChanged = false;
            snv->m_translationBkgdFinished=false;
            snv->m_loadingBkgdFinished=false;
            auto *pdf = new CPDFWorker();
            connect(this,&CSnNet::startPdfConversion,pdf,&CPDFWorker::pdfToText,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::gotText,this,&CSnNet::pdfConverted,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::error,this,&CSnNet::pdfError,Qt::QueuedConnection);
            auto *pdft = new QThread();
            connect(pdf,&CPDFWorker::finished,pdft,&QThread::quit);
            connect(pdft,&QThread::finished,pdf,&CPDFWorker::deleteLater);
            connect(pdft,&QThread::finished,pdft,&QThread::deleteLater);
            pdf->moveToThread(pdft);
            pdft->start();
            Q_EMIT startPdfConversion(url.toString());
        } else if (mime.startsWith(QSL("text/html"),Qt::CaseInsensitive) && fi.suffix().isEmpty()) {
            // for local html files without extension
            QUrl u = url;
            u.setScheme(CMagicFileSchemeHandler::getScheme());
            snv->m_fileChanged = false;
            snv->txtBrowser->load(u);
            snv->m_auxContentLoaded=false;
        } else { // for local html files
            snv->m_fileChanged = false;
            snv->txtBrowser->load(url);
            snv->m_auxContentLoaded=false;
        }
        gSet->appendRecent(fname);
    } else {
        QUrl u = url;
        CGenericFuncs::checkAndUnpackUrl(u);
        snv->m_fileChanged = false;
        snv->txtBrowser->load(u);
        snv->m_auxContentLoaded=false;
    }
    m_loadedUrl=url;
}

void CSnNet::load(const QString &html, const QUrl &baseUrl)
{
    if (html.toUtf8().size()<CDefaults::maxDataUrlFileSize) {
        snv->updateWebViewAttributes();
        snv->txtBrowser->setHtmlInterlocked(html,baseUrl);
        m_loadedUrl.clear();
        snv->m_auxContentLoaded=true;
    } else {
        loadWithTempFile(html,false);
    }
}

void CSnNet::authenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator)
{
    CAuthDlg dlg(QApplication::activeWindow(),requestUrl,authenticator->realm());
    if (dlg.exec() == QDialog::Accepted) {
        authenticator->setUser(dlg.getUser());
        authenticator->setPassword(dlg.getPassword());
    } else {
        *authenticator = QAuthenticator();
    }
}

void CSnNet::proxyAuthenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator,
                                         const QString &proxyHost)
{
    Q_UNUSED(proxyHost)
    CAuthDlg dlg(QApplication::activeWindow(),requestUrl,authenticator->realm());
    if (dlg.exec() == QDialog::Accepted) {
        authenticator->setUser(dlg.getUser());
        authenticator->setPassword(dlg.getPassword());
    } else {
        *authenticator = QAuthenticator();
    }
}
