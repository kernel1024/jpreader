#include <QMessageBox>

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QUrlQuery>
#endif

#include "sntrans.h"

CSnTrans::CSnTrans(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
}

void CSnTrans::translate()
{
    snv->calculatedUrl="";
    QString aUri="";
    snv->onceTranslated=true;
    if (gSet->translatorEngine==TE_ATLAS) {
        aUri = snv->txtBrowser->page()->mainFrame()->toHtml();
        snv->savedBaseUrl = snv->txtBrowser->page()->mainFrame()->baseUrl();
    } else {
        if (snv->txtBrowser->page()!=NULL && snv->txtBrowser->page()->mainFrame()!=NULL)
            aUri = snv->txtBrowser->page()->mainFrame()->baseUrl().toString();
        if (aUri.isEmpty() || aUri.contains("about:blank",Qt::CaseInsensitive)) aUri=snv->auxContent;
    }

    CCalcThread* ct = new CCalcThread(gSet,aUri,snv->waitDlg);
    connect(ct,SIGNAL(calcFinished(bool,QString)),this,SLOT(calcFinished(bool,QString)));
    snv->waitDlg->setProgressEnabled(false);
    if (gSet->translatorEngine==TE_ATLAS) {
        snv->waitDlg->setText(tr("Translating text with ATLAS..."));
        snv->waitDlg->setProgressEnabled(true);
        snv->waitDlg->setProgressValue(0);
    } else
        snv->waitDlg->setText(tr("Copying file to hosting..."));
    snv->txtPanel->hide();
    snv->waitDlg->show();
    gSet->appendTranThread(ct);
}

void CSnTrans::calcFinished(bool success, QString aUrl)
{
//    QWidget* fw = QApplication::focusWidget();
    snv->waitDlg->hide();
    snv->txtPanel->show();
    if (success) {
        snv->calculatedUrl=aUrl;
        postTranslate();
        snv->parentWnd->updateTabs();
    } else {
        if (aUrl.contains("ERROR:ATLAS_SLIPPED"))
            QMessageBox::warning(snv,tr("JPReader error"),tr("ATLAS slipped. Please restart translation."));
        else
            QMessageBox::warning(snv,tr("JPReader error"),tr("Url not calculated. Network error occured."));
    }
}

void CSnTrans::postTranslate()
{
    if (snv->calculatedUrl.isEmpty()) return;
    if (snv->calculatedUrl.contains("ERROR:ATLAS_SLIPPED")) {
        QMessageBox::warning(snv,tr("JPReader error"),tr("ATLAS slipped. Please restart translation."));
        return;
    }
    QNetworkRequest rqst;
    QByteArray postBody;
    QUrl url;
    QString cn;
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QUrlQuery qu;
#endif
    switch (gSet->translatorEngine) {
    case TE_NIFTY:
        // POST requester
        rqst.setUrl(QUrl("http://honyaku-result.nifty.com/LUCNIFTY/ns/wt_ex.cgi"));
        rqst.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
        postBody=QByteArray("SLANG=ja&TLANG=en&XMODE=1&SURL=");
        postBody.append(QUrl::toPercentEncoding(snv->calculatedUrl));
        snv->onceLoaded=true;
        snv->txtBrowser->load(rqst,QNetworkAccessManager::PostOperation,postBody);
        if (snv->tabWidget->currentWidget()==snv) snv->txtBrowser->setFocus();
        break;
    case TE_GOOGLE:
        url = QUrl("http://translate.google.com/translate");
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
        url.addQueryItem("sl","ja");
        url.addQueryItem("tl","en");
        url.addQueryItem("u",snv->calculatedUrl);
#else
        qu.addQueryItem("sl","ja");
        qu.addQueryItem("tl","en");
        qu.addQueryItem("u",snv->calculatedUrl);
        url.setQuery(qu);
#endif
        rqst.setUrl(url);
        rqst.setRawHeader("Referer",url.toString().toUtf8());
        snv->onceLoaded=true;
        snv->txtBrowser->load(rqst,QNetworkAccessManager::GetOperation);
        if (snv->tabWidget->currentWidget()==snv) snv->txtBrowser->setFocus();
        break;
    case TE_ATLAS: // Url contains translated file itself
        cn = snv->calculatedUrl;
        snv->fileChanged = true;
        snv->txtBrowser->setHtml(cn,snv->savedBaseUrl);
        if (snv->tabWidget->currentWidget()==snv) snv->txtBrowser->setFocus();
        break;
    default:
        QMessageBox::warning(snv,tr("JPReader"),tr("Unknown translation engine selected"));
        return;
    }
    if (snv->parentWnd->tabMain->currentIndex()!=snv->parentWnd->tabMain->indexOf(snv) &&
            !snv->loadingBkgdFinished) { // tab is inactive
        snv->translationBkgdFinished=true;
        snv->parentWnd->tabMain->tabBar()->setTabTextColor(snv->parentWnd->tabMain->indexOf(snv),QColor("green"));
        snv->parentWnd->updateTabs();
    }
}

void CSnTrans::progressLoad(int progress)
{
    snv->barLoading->setValue(progress);
    if (!snv->onceLoaded && progress!=1244512) return;
    if (gSet->translatorEngine==TE_NIFTY) {
        if (snv->txtBrowser->page()->mainFrame()->childFrames().count()>1) {
            if (snv->txtBrowser->page()->mainFrame()->childFrames()[1]->baseUrl().toString().contains("LUCNIFTY")) {
                snv->onceLoaded=false;
                snv->netHandler->loadProcessed(snv->txtBrowser->page()->mainFrame()->childFrames()[1]->baseUrl());
                snv->txtBrowser->setFocus();
                snv->fileChanged = true;
            }
        }
    } else if (gSet->translatorEngine==TE_GOOGLE) {
        if (snv->txtBrowser->page()->mainFrame()->childFrames().count()>1) {
            if (snv->txtBrowser->page()->mainFrame()->childFrames()[1]->baseUrl().toString().contains("translate_p")) {
                snv->onceLoaded=false;
                snv->netHandler->loadProcessed(snv->txtBrowser->page()->mainFrame()->childFrames()[1]->baseUrl());
                snv->txtBrowser->setFocus();
                snv->fileChanged = true;
            }
        }
    }
}
