#include <QTextCodec>
#include <QPointer>
#include <QMessageBox>
#include <QFileInfo>
#include <QWebEngineScriptCollection>
#include <QDirIterator>
#include <QDialog>
#include <QThread>
#include <QAuthenticator>

#include "snnet.h"
#include "snmsghandler.h"
#include "genericfuncs.h"
#include "authdlg.h"
#include "pdftotext.h"
#include "downloadmanager.h"
#include "genericfuncs.h"
#include "pixivnovelextractor.h"
#include "pixivindexextractor.h"
#include "globalcontrol.h"
#include "ui_selectablelistdlg.h"

namespace CDefaults {
const int browserPageFinishedTimeoutMS = 30000;
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

void CSnNet::multiImgDownload(const QStringList &urls, const QUrl& referer, const QString& preselectedName)
{
    static QSize multiImgDialogSize = QSize();

    if (gSet->downloadManager()==nullptr) return;

    QDialog dlg(snv);
    Ui::SelectableListDlg ui;
    ui.setupUi(&dlg);
    if (multiImgDialogSize.isValid())
        dlg.resize(multiImgDialogSize);

    connect(ui.filter,&QLineEdit::textEdited,[ui](const QString& text){
        if (text.isEmpty()) {
            for (int i=0;i<ui.list->count();i++) {
                if (ui.list->item(i)->isHidden())
                    ui.list->item(i)->setHidden(false);
            }
            return;
        }

        for (int i=0;i<ui.list->count();i++) {
            ui.list->item(i)->setHidden(true);
        }
        const QList<QListWidgetItem *> filtered = ui.list->findItems(text,
                                                                     ((ui.syntax->currentIndex()==0) ?
                                                                          Qt::MatchRegExp : Qt::MatchWildcard));
        for (QListWidgetItem* item : filtered) {
            item->setHidden(false);
        }
    });

    ui.label->setText(tr("Detected image URLs from page"));
    ui.syntax->setCurrentIndex(0);
    ui.list->addItems(urls);
    dlg.setWindowTitle(tr("Multiple images download"));
    if (!preselectedName.isEmpty())
        ui.list->selectAll();

    if (dlg.exec()==QDialog::Accepted) {
        QString dir;
        if (ui.checkZipFile->isChecked()) {
            QString fname = preselectedName;
            if (!fname.endsWith(QSL(".zip"),Qt::CaseInsensitive))
                fname += QSL(".zip");
            dir = CGenericFuncs::getSaveFileNameD(snv,tr("Pack images to ZIP file"),CGenericFuncs::getTmpDir(),
                                                  QStringList( { tr("ZIP file (*.zip)") } ),nullptr,fname);
        } else {
            dir = CGenericFuncs::getExistingDirectoryD(snv,tr("Save images to directory"),
                                                       CGenericFuncs::getTmpDir());
        }
        int index = 0;
        const QList<QListWidgetItem *> itml = ui.list->selectedItems();
        for (const QListWidgetItem* itm : itml){
            if (!ui.checkAddNumbers->isChecked()) {
                index = -1;
            } else {
                index++;
            }

            gSet->downloadManager()->handleAuxDownload(itm->text(),dir,referer,index,ui.list->selectedItems().count());
        }
    }
    multiImgDialogSize=dlg.size();
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
            auto sv = new CSnippetViewer(snv->parentWnd(),QUrl::fromLocalFile(fname));
            sv->m_requestAutotranslate = autoTranslate;
        } else {
            snv->m_fileChanged = false;
            snv->m_requestAutotranslate = autoTranslate;
            snv->txtBrowser->load(QUrl::fromLocalFile(fname));
            snv->m_auxContentLoaded=false;
        }
        return true;
    }

    QMessageBox::warning(snv,tr("JPReader"),tr("Unable to create temporary file "
                                               "for document."));
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

void CSnNet::processPixivNovel(const QUrl &url, const QString& title, bool translate, bool focus)
{
    auto ex = new CPixivNovelExtractor();
    auto th = new QThread();
    ex->setParams(snv,url,title,translate,focus);

    connect(ex,&CPixivNovelExtractor::novelReady,this,&CSnNet::novelReady,Qt::QueuedConnection);
    connect(ex,&CPixivNovelExtractor::finished,th,&QThread::quit);
    connect(th,&QThread::finished,ex,&CPixivNovelExtractor::deleteLater);
    connect(th,&QThread::finished,th,&QThread::deleteLater);

    ex->moveToThread(th);
    th->start();

    gSet->app()->setOverrideCursor(Qt::BusyCursor);

    QMetaObject::invokeMethod(ex,&CPixivNovelExtractor::start,Qt::QueuedConnection);
}

void CSnNet::processPixivNovelList(const QString& pixivId, CPixivIndexExtractor::IndexMode mode)
{
    auto ex = new CPixivIndexExtractor();
    auto th = new QThread();
    ex->setParams(snv,pixivId);

    connect(ex,&CPixivIndexExtractor::listReady,this,&CSnNet::pixivListReady,Qt::QueuedConnection);
    connect(ex,&CPixivIndexExtractor::finished,th,&QThread::quit);
    connect(th,&QThread::finished,ex,&CPixivIndexExtractor::deleteLater);
    connect(th,&QThread::finished,th,&QThread::deleteLater);

    ex->moveToThread(th);
    th->start();

    gSet->app()->setOverrideCursor(Qt::BusyCursor);

    switch (mode) {
        case CPixivIndexExtractor::WorkIndex:
            QMetaObject::invokeMethod(ex,&CPixivIndexExtractor::createNovelList,Qt::QueuedConnection);
            break;
        case CPixivIndexExtractor::BookmarksIndex:
            QMetaObject::invokeMethod(ex,&CPixivIndexExtractor::createNovelBookmarksList,Qt::QueuedConnection);
            break;
        default:
            qCritical() << "Unknown pixiv index mode " << mode;
    }
}

void CSnNet::downloadPixivManga()
{
    auto ac = qobject_cast<QAction*>(sender());
    if (!ac) return;
    QUrl origin = ac->data().toUrl();
    if (!origin.isValid()) return;

    auto ex = new CPixivNovelExtractor();
    auto th = new QThread();
    ex->setMangaOrigin(snv,origin);

    connect(ex,&CPixivNovelExtractor::mangaReady,this,&CSnNet::pixivMangaReady,Qt::QueuedConnection);
    connect(ex,&CPixivNovelExtractor::finished,th,&QThread::quit);
    connect(th,&QThread::finished,ex,&CPixivNovelExtractor::deleteLater);
    connect(th,&QThread::finished,th,&QThread::deleteLater);

    ex->moveToThread(th);
    th->start();

    gSet->app()->setOverrideCursor(Qt::BusyCursor);

    QMetaObject::invokeMethod(ex,&CPixivNovelExtractor::startManga,Qt::QueuedConnection);
}

void CSnNet::novelReady(const QString &html, bool focus, bool translate)
{
    gSet->app()->restoreOverrideCursor();

    if (html.toUtf8().size()<CDefaults::maxDataUrlFileSize) {
        auto sv = new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),focus,html);
        sv->m_requestAutotranslate = translate;
    } else {
        loadWithTempFile(html,true,translate);
    }
}

void CSnNet::pixivListReady(const QString &html)
{
    gSet->app()->restoreOverrideCursor();

    if (html.toUtf8().size()<CDefaults::maxDataUrlFileSize) {
        new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),true,html);
    } else {
        loadWithTempFile(html,true);
    }
}

void CSnNet::pixivMangaReady(const QStringList &urls, const QString &id, const QUrl &origin)
{
    gSet->app()->restoreOverrideCursor();

    if (urls.isEmpty() || id.isEmpty()) {
        QMessageBox::warning(snv,tr("JPReader"),
                             tr("No pixiv manga image urls found"));
    } else {
        snv->netHandler->multiImgDownload(urls,origin,id);
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
    QMessageBox::critical(snv,tr("JPReader"),tr("Unable to open PDF file."));
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
            QMessageBox::critical(snv,tr("JPReader"),tr("Unable to open file."));
            return;
        }
        QString MIME = CGenericFuncs::detectMIME(fname);
        if (MIME.startsWith(QSL("text/plain"),Qt::CaseInsensitive) &&
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
        } else if (MIME.startsWith(QSL("application/pdf"),Qt::CaseInsensitive)) {
            // for local pdf files
            snv->m_fileChanged = false;
            snv->m_translationBkgdFinished=false;
            snv->m_loadingBkgdFinished=false;
            auto pdf = new CPDFWorker();
            connect(this,&CSnNet::startPdfConversion,pdf,&CPDFWorker::pdfToText,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::gotText,this,&CSnNet::pdfConverted,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::error,this,&CSnNet::pdfError,Qt::QueuedConnection);
            auto pdft = new QThread();
            connect(pdf,&CPDFWorker::finished,pdft,&QThread::quit);
            connect(pdft,&QThread::finished,pdf,&CPDFWorker::deleteLater);
            connect(pdft,&QThread::finished,pdft,&QThread::deleteLater);
            pdf->moveToThread(pdft);
            pdft->start();
            Q_EMIT startPdfConversion(url.toString());
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
