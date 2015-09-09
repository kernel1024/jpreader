#include <QTextCodec>
#include <QPointer>
#include <QMessageBox>
#include <QFileInfo>
#include "snnet.h"
#include "genericfuncs.h"
#include "authdlg.h"

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
    snv->msgHandler->hideBarLoading();
    snv->loading = false;
    snv->updateButtonsState();

    if (snv->parentWnd->tabMain->currentIndex()!=snv->parentWnd->tabMain->indexOf(snv) &&
            !snv->translationBkgdFinished) { // tab is inactive
        snv->loadingBkgdFinished=true;
        snv->parentWnd->tabMain->tabBar()->setTabTextColor(snv->parentWnd->tabMain->indexOf(snv),Qt::blue);
        snv->parentWnd->updateTabs();
    }

    if ((gSet->autoTranslate() || snv->requestAutotranslate) && !snv->onceTranslated) {
        snv->requestAutotranslate = false;
        snv->transButton->click();
    }

    if (snv->tabWidget->currentWidget()==snv) snv->txtBrowser->setFocus();
}

void CSnNet::userNavigationRequest(const QUrl &, const int)
{
    snv->onceTranslated = false;
    snv->fileChanged = false;
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
        if (!fi.exists()) {
            QString cn=makeSimpleHtml(tr("Error"),tr("Unable to find file '%1'.").arg(fname));
            snv->fileChanged = false;
            snv->translationBkgdFinished=false;
            snv->loadingBkgdFinished=false;
            snv->txtBrowser->setHtml(cn,url);
            QMessageBox::critical(snv,tr("JPReader"),tr("Unable to open file. File not found."));
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
        } else { // for local html files
            snv->fileChanged = false;
            snv->txtBrowser->load(url);
        }
    } else {
        snv->fileChanged = false;
        snv->txtBrowser->load(url);
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
    }
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
    }
    dlg->setParent(NULL);
    delete dlg;
}
