#include <QTextCodec>
#include <QPointer>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrlQuery>
#include <QWebEngineScriptCollection>
#include <QDirIterator>
#include <QDialog>
#include "snnet.h"
#include "genericfuncs.h"
#include "authdlg.h"
#include "pdftotext.h"
#include "downloadmanager.h"
#include "ui_selectablelistdlg.h"

CSnNet::CSnNet(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    loadedUrl.clear();
}

void CSnNet::multiImgDownload(const QStringList &urls, const QUrl& referer)
{
    static QSize multiImgDialogSize = QSize();

    QDialog *dlg = new QDialog(snv);
    Ui::SelectableListDlg ui;
    ui.setupUi(dlg);
    if (multiImgDialogSize.isValid())
        dlg->resize(multiImgDialogSize);

    ui.label->setText(tr("Detected image URLs from page"));
    ui.list->addItems(urls);
    ui.list->selectAll();
    dlg->setWindowTitle(tr("Multiple images download"));

    if (dlg->exec()==QDialog::Accepted) {
        QString dir = getExistingDirectoryD(snv,tr("Save images to directory"),getTmpDir());
        foreach(const QListWidgetItem* itm, ui.list->selectedItems()){
            gSet->downloadManager->handleAuxDownload(itm->text(),dir,referer);
        }
    }
    multiImgDialogSize=dlg->size();
    dlg->setParent(nullptr);
    dlg->deleteLater();
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
        checkAndUnpackUrl(u);
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
    dlg->setParent(nullptr);
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
    dlg->setParent(nullptr);
    delete dlg;
}
