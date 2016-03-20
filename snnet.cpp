#include <QTextCodec>
#include <QPointer>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrlQuery>
#include "snnet.h"
#include "genericfuncs.h"
#include "authdlg.h"
#include "pdftotext.h"

CSnNet::CSnNet(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    loadedUrl.clear();
}

void CSnNet::loadStarted()
{
    snv->barLoading->setValue(0);
    snv->barLoading->show();
    snv->barPlaceholder->hide();
    snv->loading = true;
    snv->updateButtonsState();
}

void CSnNet::loadFinished(bool)
{
    snv->msgHandler->updateZoomFactor();
    snv->msgHandler->hideBarLoading();
    snv->loading = false;
    snv->updateButtonsState();

    if (snv->parentWnd->tabMain->currentIndex()!=snv->parentWnd->tabMain->indexOf(snv) &&
            !snv->translationBkgdFinished) { // tab is inactive
        snv->loadingBkgdFinished=true;
        snv->parentWnd->tabMain->tabBar()->setTabTextColor(snv->parentWnd->tabMain->indexOf(snv),Qt::blue);
        snv->parentWnd->updateTabs();
    }

    if ((gSet->ui.autoTranslate() || snv->requestAutotranslate) && !snv->onceTranslated) {
        snv->requestAutotranslate = false;
        snv->transButton->click();
    }
    snv->pageLoaded = true;

    if (snv->tabWidget->currentWidget()==snv) {
        snv->takeScreenshot();
        snv->txtBrowser->setFocus();
    }
}

void CSnNet::userNavigationRequest(const QUrl &url, const int type, const bool isMainFrame)
{
    Q_UNUSED(type);

    snv->onceTranslated = false;
    snv->fileChanged = false;

    if (isMainFrame) {
        UrlHolder uh(QString("(blank)"),url);
        gSet->appendMainHistory(uh);
    }
}

void CSnNet::iconUrlChanged(const QUrl &url)
{
    if (!gSet->settings.showFavicons) return;
    if (url.isEmpty() || !url.isValid()) return;

    CFaviconLoader* fl = new CFaviconLoader(snv,url);
    connect(fl,&CFaviconLoader::gotIcon,snv,&CSnippetViewer::updateTabIcon);
    fl->queryStart(false);
}

void CSnNet::load(const QUrl &url)
{
    if (gSet->isUrlBlocked(url)) {
        qInfo() << "adblock - skipping" << url;
        return;
    }

    snv->updateWebViewAttributes();
    if (snv->isStartPage)
        snv->isStartPage = false;

    QString fname = url.toLocalFile();
    if (!fname.isEmpty()) {
        QFileInfo fi(fname);
        if (!fi.exists() || !fi.isReadable()) {
            QString cn=makeSimpleHtml(tr("Error"),tr("Unable to find file '%1'.").arg(fname));
            snv->fileChanged = false;
            snv->translationBkgdFinished=false;
            snv->loadingBkgdFinished=false;
            snv->txtBrowser->setHtml(cn,url);
            QMessageBox::critical(snv,tr("JPReader"),tr("Unable to open file."));
            return;
        }
        QString MIME = detectMIME(fname);
        if (MIME.startsWith("text/plain",Qt::CaseInsensitive)) { // for local txt files
            QFile data(fname);
            if (data.open(QFile::ReadOnly)) {
                QByteArray ba = data.readAll();
                QTextCodec* cd = detectEncoding(ba);
                QString cn=makeSimpleHtml(fi.fileName(),cd->toUnicode(ba));
                snv->fileChanged = false;
                snv->translationBkgdFinished=false;
                snv->loadingBkgdFinished=false;
                snv->txtBrowser->setHtml(cn,url);
                data.close();
            }
        } else if (MIME.startsWith("application/pdf",Qt::CaseInsensitive)) { // for local pdf files
            QString cn;
            snv->fileChanged = false;
            snv->translationBkgdFinished=false;
            snv->loadingBkgdFinished=false;
            if (!pdfToText(url,cn)) {
                QString cn=makeSimpleHtml(tr("Error"),tr("Unable to open PDF file '%1'.").arg(fname));
                snv->txtBrowser->setHtml(cn,url);
                QMessageBox::critical(snv,tr("JPReader"),tr("Unable to open PDF file."));
            } else
                snv->txtBrowser->setHtml(cn,url);
        } else { // for local html files
            snv->fileChanged = false;
            snv->txtBrowser->load(url);
        }
        gSet->appendRecent(fname);
    } else {
        QUrl u = url;
        if (u.host().endsWith("pixiv.net") && u.path().startsWith("/jump")) { // Extract jump url for pixiv
            QUrl ju(u.query(QUrl::FullyDecoded));
            if (ju.isValid())
                u = ju;
        }
        snv->fileChanged = false;
        snv->txtBrowser->load(u);
    }
    loadedUrl=url;
}

void CSnNet::load(const QString &html, const QUrl &baseUrl)
{
    snv->updateWebViewAttributes();
    snv->txtBrowser->setHtml(html,baseUrl);
    loadedUrl.clear();
}

void CSnNet::authenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator)
{
    CAuthDlg *dlg = new CAuthDlg(QApplication::activeWindow(),requestUrl,authenticator->realm());
    if (dlg->exec()) {
        authenticator->setUser(dlg->getUser());
        authenticator->setPassword(dlg->getPassword());
    } else
        *authenticator = QAuthenticator();
    dlg->setParent(NULL);
    delete dlg;
}

void CSnNet::proxyAuthenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator,
                                         const QString &proxyHost)
{
    Q_UNUSED(proxyHost);
    CAuthDlg *dlg = new CAuthDlg(QApplication::activeWindow(),requestUrl,authenticator->realm());
    if (dlg->exec()) {
        authenticator->setUser(dlg->getUser());
        authenticator->setPassword(dlg->getPassword());
    } else
        *authenticator = QAuthenticator();
    dlg->setParent(NULL);
    delete dlg;
}
