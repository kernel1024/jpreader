#include <QPointer>
#include <QMessageBox>
#include <QFileInfo>
#include <QWebEngineScriptCollection>
#include <QDirIterator>
#include <QDialog>
#include <QThread>
#include <QMimeData>
#include <QAuthenticator>

#include "net.h"
#include "msghandler.h"
#include "utils/genericfuncs.h"
#include "utils/pdftotext.h"
#include "browser-utils/authdlg.h"
#include "extractors/htmlimagesextractor.h"
#include "global/control.h"
#include "global/startup.h"
#include "global/contentfiltering.h"
#include "global/history.h"

namespace CDefaults {
const int browserPageFinishedTimeoutMS = 30000;
const int clipboardHtmlAquireDelay = 1000;
}

CBrowserNet::CBrowserNet(CBrowserTab *parent)
    : QObject(parent),
      snv(parent)
{
    m_finishedTimer.setInterval(CDefaults::browserPageFinishedTimeoutMS);
    m_finishedTimer.setSingleShot(true);
    connect(&m_finishedTimer,&QTimer::timeout,this,[this](){
        loadFinished(true);
    });
}

bool CBrowserNet::isValidLoadedUrl(const QUrl& url)
{
    // loadedUrl points to non-empty page
    if (!url.isValid()) return false;
    if (!url.toLocalFile().isEmpty()) return true;
    if (url.scheme().startsWith(QSL("http"),Qt::CaseInsensitive)) return true;
    return false;
}

bool CBrowserNet::isValidLoadedUrl()
{
    return isValidLoadedUrl(m_loadedUrl);
}

void CBrowserNet::progressLoad(int progress)
{
    snv->barLoading->setValue(progress);
    m_finishedTimer.start();
}

void CBrowserNet::loadStarted()
{
    snv->barLoading->setValue(0);
    snv->barLoading->show();
    snv->barPlaceholder->hide();
    snv->m_loading = true;
    snv->updateButtonsState();
}

void CBrowserNet::loadFinished(bool ok)
{
    if (!snv->m_loading) return;

    snv->m_msgHandler->updateZoomFactor();
    snv->m_msgHandler->hideBarLoading();
    snv->m_loading = false;
    snv->updateButtonsState();

    if (isValidLoadedUrl(snv->txtBrowser->url()) && ok) {
        snv->m_auxContentLoaded=false;
        m_loadedUrl = snv->txtBrowser->url();
    }

    snv->updateTabColor(true,false);

    bool xorAutotranslate = (gSet->actions()->autoTranslate() && !snv->m_requestAutotranslate) ||
                            (!gSet->actions()->autoTranslate() && snv->m_requestAutotranslate);

    if (xorAutotranslate && !snv->m_onceTranslated && (isValidLoadedUrl() || snv->m_auxContentLoaded))
        snv->transButton->click();

    snv->m_pageLoaded = true;
    snv->m_requestAutotranslate = false;

    if (snv->tabWidget()->currentWidget()==snv) {
        snv->takeScreenshot();
        snv->txtBrowser->setFocus();
    }
}

void CBrowserNet::userNavigationRequest(const QUrl &url, int type, bool isMainFrame)
{
    Q_UNUSED(type)

    if (!isValidLoadedUrl(url)) return;

    snv->m_onceTranslated = false;
    snv->m_auxContentLoaded=false;

    if (isMainFrame) {
        CUrlHolder uh(QSL("(blank)"),url);
        gSet->history()->appendMainHistory(uh);
    }
}

void CBrowserNet::extractHTMLFragment()
{
    if (!snv->txtBrowser->hasSelection()) return;

    snv->txtBrowser->page()->action(QWebEnginePage::Copy)->activate(QAction::Trigger);
    const QUrl url = snv->txtBrowser->url();
    QTimer::singleShot(CDefaults::clipboardHtmlAquireDelay,this,[this,url](){
        QClipboard *cb = QApplication::clipboard();
        const QMimeData *md = cb->mimeData(QClipboard::Clipboard);
        if (md->hasHtml()) {
            auto *ex = new CHtmlImagesExtractor(nullptr);
            ex->setParams(md->html(),url,false,false,true);
            if (!gSet->startup()->setupThreadedWorker(ex)) {
                delete ex;
            } else {
                QMetaObject::invokeMethod(ex,&CAbstractThreadWorker::start,Qt::QueuedConnection);
            }
        } else {
            QMessageBox::information(snv,QGuiApplication::applicationDisplayName(),
                                     tr("Unknown clipboard format. HTML expected."));
        }
        cb->clear(QClipboard::Clipboard);
    });
}

void CBrowserNet::processExtractorAction()
{
    auto *ac = qobject_cast<QAction*>(sender());
    if (!ac) return;

    processExtractorActionIndirect(ac->data().toHash());
}

void CBrowserNet::processExtractorActionIndirect(const QVariantHash &params)
{
    auto *ex = CAbstractExtractor::extractorFactory(params,snv);
    if (ex == nullptr) return;
    if (!gSet->startup()->setupThreadedWorker(ex)) {
        delete ex;
        return;
    }
    QMetaObject::invokeMethod(ex,&CAbstractExtractor::start,Qt::QueuedConnection);
}

void CBrowserNet::pdfConverted(const QString &html)
{
    QString page = html;
    if (page.isEmpty()) {
        page = CGenericFuncs::makeSimpleHtml(QSL("PDF conversion error"),
                                             QSL("Empty document."));
    }
    load(page,QUrl(),false,false,false);
}

void CBrowserNet::pdfError(const QString &message)
{
    QString cn=CGenericFuncs::makeSimpleHtml(tr("Error"),tr("Unable to open PDF file.<br>%1").arg(message));
    snv->txtBrowser->setHtmlInterlocked(cn);
    QMessageBox::critical(snv,QGuiApplication::applicationDisplayName(),
                          tr("Unable to open PDF file."));
}

void CBrowserNet::load(const QUrl &url, bool autoTranslate, bool alternateAutoTranslate)
{
    if (gSet->contentFilter()->isUrlBlocked(url)) {
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
            snv->m_onceTranslated = false;
            snv->m_translationBkgdFinished=false;
            snv->m_loadingBkgdFinished=false;
            snv->txtBrowser->setHtmlInterlocked(cn,url);
            QMessageBox::critical(snv,QGuiApplication::applicationDisplayName(),
                                  tr("Unable to open file."));
            return;
        }
        QString mime = CGenericFuncs::detectMIME(fname);
        if (mime.startsWith(QSL("text/plain"),Qt::CaseInsensitive) &&
                fi.size()<CDefaults::maxDataUrlFileSize) {
            // for small local txt files (load big files via file url directly)
            QFile data(fname);
            if (data.open(QFile::ReadOnly)) {
                QByteArray ba = data.readAll();
                QString cn=CGenericFuncs::makeSimpleHtml(fi.fileName(),
                                                         CGenericFuncs::detectDecodeToUnicode(ba));
                snv->m_onceTranslated = false;
                snv->m_translationBkgdFinished=false;
                snv->m_loadingBkgdFinished=false;
                snv->txtBrowser->setHtmlInterlocked(cn,url);
                snv->m_auxContentLoaded=false;
                data.close();
            }
        } else if (mime.startsWith(QSL("application/pdf"),Qt::CaseInsensitive)) {
            // for local pdf files
            snv->m_onceTranslated = false;
            snv->m_translationBkgdFinished=false;
            snv->m_loadingBkgdFinished=false;
            auto *pdf = new CPDFWorker();
            auto *pdft = new QThread();
            pdf->moveToThread(pdft);
            connect(this,&CBrowserNet::startPdfConversion,pdf,&CPDFWorker::pdfToText,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::gotText,this,&CBrowserNet::pdfConverted,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::error,this,&CBrowserNet::pdfError,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::finished,pdft,&QThread::quit);
            connect(pdft,&QThread::finished,pdf,&CPDFWorker::deleteLater);
            connect(pdft,&QThread::finished,pdft,&QThread::deleteLater);
            pdft->setObjectName(QSL("PDF_parser"));
            pdft->start();
            Q_EMIT startPdfConversion(url.toString());
        } else if (mime.startsWith(QSL("text/html"),Qt::CaseInsensitive) && fi.suffix().isEmpty()) {
            // for local html files without extension
            QUrl u = url;
            u.setScheme(CMagicFileSchemeHandler::getScheme());
            snv->m_onceTranslated = false;
            snv->txtBrowser->load(u);
            snv->m_auxContentLoaded=false;
        } else { // for local html files
            snv->m_onceTranslated = false;
            snv->txtBrowser->load(url);
            snv->m_auxContentLoaded=false;
        }
        gSet->history()->appendRecent(fname);
    } else {
        QUrl u = url;
        CGenericFuncs::checkAndUnpackUrl(u);
        snv->m_onceTranslated = false;
        snv->txtBrowser->load(u);
        snv->m_auxContentLoaded=false;
    }
    m_loadedUrl=url;
    snv->m_requestAutotranslate = autoTranslate;
    snv->m_requestAlternateAutotranslate = alternateAutoTranslate;
}

void CBrowserNet::load(const QString &html, const QUrl &baseUrl, bool autoTranslate, bool alternateAutoTranslate,
                       bool onceTranslated)
{
    if (html.toUtf8().size()<CDefaults::maxDataUrlFileSize) {
        snv->updateWebViewAttributes();
        snv->m_onceTranslated = onceTranslated;
        snv->txtBrowser->setHtmlInterlocked(html,baseUrl);
        if (!snv->m_onceTranslated) {
            m_loadedUrl.clear();
            snv->m_auxContentLoaded=true;
        } else {
            m_loadedUrl = baseUrl;
        }

    } else { // load big files with tmp file - chromium dataurl 2Mb limitation, QTBUG-53414
        QString fname = gSet->makeTmpFile(QSL("html"),html);
        if (fname.isEmpty()) {
            QMessageBox::critical(snv, QGuiApplication::applicationDisplayName(),
                                  tr("Unable to create temp file."));
            return;
        }

        snv->m_onceTranslated = onceTranslated;
        snv->txtBrowser->load(QUrl::fromLocalFile(fname));
        snv->m_auxContentLoaded=false;
        m_loadedUrl = baseUrl;
    }

    snv->m_requestAutotranslate = autoTranslate;
    snv->m_requestAlternateAutotranslate = alternateAutoTranslate;
    snv->urlChanged(m_loadedUrl); // update tab colors
}

void CBrowserNet::authenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator)
{
    CAuthDlg dlg(QApplication::activeWindow(),requestUrl,authenticator->realm());
    if (dlg.exec() == QDialog::Accepted) {
        authenticator->setUser(dlg.getUser());
        authenticator->setPassword(dlg.getPassword());
    } else {
        *authenticator = QAuthenticator();
    }
}

void CBrowserNet::proxyAuthenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator,
                                         const QString &proxyHost)
{
    Q_UNUSED(requestUrl)
    CAuthDlg dlg(QApplication::activeWindow(),proxyHost,authenticator->realm());
    if (dlg.exec() == QDialog::Accepted) {
        authenticator->setUser(dlg.getUser());
        authenticator->setPassword(dlg.getPassword());
    } else {
        *authenticator = QAuthenticator();
    }
}
