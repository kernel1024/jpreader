#include <QMessageBox>

#include <QUrlQuery>

#include "sntrans.h"
#include "globalcontrol.h"
#include "goldendictmgr.h"
#include "translator.h"
#include "qxttooltip.h"

CSnTrans::CSnTrans(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;

    selectionTimer = new QTimer(this);
    selectionTimer->setInterval(1000);
    selectionTimer->setSingleShot(true);

    connect(snv->txtBrowser->page(), SIGNAL(selectionChanged()),this,SLOT(selectionChanged()));
    connect(selectionTimer, SIGNAL(timeout()),this,SLOT(selectionShow()));
}

void CSnTrans::convertToXML()
{
    snv->calculatedUrl="";
    snv->onceTranslated=true;
    QString aUri = snv->txtBrowser->page()->mainFrame()->toHtml();
    snv->savedBaseUrl = snv->txtBrowser->page()->mainFrame()->baseUrl();
    if (snv->savedBaseUrl.hasFragment())
        snv->savedBaseUrl.setFragment(QString());

    CTranslator* ct = new CTranslator(NULL,aUri,NULL);
    QString res;
    if (!ct->documentToXML(aUri,res)) {
        QMessageBox::critical(snv,tr("JPReader error"),tr("Translation to XML failed."));
        return;
    }

    snv->calculatedUrl=res;
    postTranslate();
    snv->parentWnd->updateTabs();
}

void CSnTrans::translate()
{
    snv->calculatedUrl="";
    QString aUri="";
    snv->onceTranslated=true;
    snv->waitPanel->show();
    snv->transButton->setEnabled(false);
    if (gSet->translatorEngine==TE_ATLAS ||
            gSet->translatorEngine==TE_BINGAPI) {
        aUri = snv->txtBrowser->page()->mainFrame()->toHtml();
        snv->savedBaseUrl = snv->txtBrowser->page()->mainFrame()->baseUrl();
        if (snv->savedBaseUrl.hasFragment())
            snv->savedBaseUrl.setFragment(QString());
    } else {
        if (snv->txtBrowser->page()!=NULL && snv->txtBrowser->page()->mainFrame()!=NULL)
            aUri = snv->txtBrowser->page()->mainFrame()->baseUrl().toString();
        if (aUri.isEmpty() || aUri.contains("about:blank",Qt::CaseInsensitive))
            aUri=snv->Uri.toString();
        if (aUri.isEmpty() || aUri.contains("about:blank",Qt::CaseInsensitive))
            aUri=snv->auxContent;
    }

    CTranslator* ct = new CTranslator(NULL,aUri,snv->waitHandler);
    QThread* th = new QThread();
    connect(ct,SIGNAL(calcFinished(bool,QString)),
            this,SLOT(calcFinished(bool,QString)),Qt::QueuedConnection);
    snv->waitHandler->setProgressValue(0);
    if (gSet->translatorEngine==TE_ATLAS) {
        snv->waitHandler->setText(tr("Translating text with ATLAS..."));
        snv->waitHandler->setProgressEnabled(true);
    } else if (gSet->translatorEngine==TE_BINGAPI) {
        snv->waitHandler->setText(tr("Translating text with Bing API..."));
        snv->waitHandler->setProgressEnabled(true);
    } else {
        snv->waitHandler->setText(tr("Copying file to hosting..."));
        snv->waitHandler->setProgressEnabled(false);
    }

    connect(gSet,SIGNAL(stopTranslators()),
            ct,SLOT(abortTranslator()),Qt::QueuedConnection);
    connect(snv->abortBtn,SIGNAL(clicked()),
            ct,SLOT(abortTranslator()),Qt::QueuedConnection);

    ct->moveToThread(th);
    th->start();

    QMetaObject::invokeMethod(ct,"translate",Qt::QueuedConnection);
}

void CSnTrans::calcFinished(const bool success, const QString& aUrl)
{
    snv->waitPanel->hide();
    snv->transButton->setEnabled(true);
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
    QUrlQuery qu;
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
        qu.addQueryItem("sl",gSet->getSourceLanguageID());
        qu.addQueryItem("tl","en");
        qu.addQueryItem("u",snv->calculatedUrl);
        url.setQuery(qu);
        rqst.setUrl(url);
        rqst.setRawHeader("Referer",url.toString().toUtf8());
        snv->onceLoaded=true;
        snv->txtBrowser->load(rqst,QNetworkAccessManager::GetOperation);
        if (snv->tabWidget->currentWidget()==snv) snv->txtBrowser->setFocus();
        break;
    case TE_ATLAS: // Url contains translated file itself
    case TE_BINGAPI:
        cn = snv->calculatedUrl;
        snv->fileChanged = true;
        snv->onceTranslated = true;
        snv->netHandler->addUrlToProcessing(snv->savedBaseUrl);
        snv->txtBrowser->setHtml(cn,snv->savedBaseUrl);
        snv->netHandler->removeUrlFromProcessing(snv->savedBaseUrl);
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

void CSnTrans::selectionChanged()
{
    storedSelection = snv->txtBrowser->page()->selectedText();
    if (!storedSelection.isEmpty() && gSet->actionSelectionDictionary->isChecked())
        selectionTimer->start();
}

void CSnTrans::findWordTranslation(const QString &text)
{
    QUrl req;
    req.setScheme( "gdlookup" );
    req.setHost( "localhost" );
    QUrlQuery requ;
    requ.addQueryItem( "word", text );
    req.setQuery(requ);
    QNetworkReply* rep = gSet->dictNetMan->get(QNetworkRequest(req));
    connect(rep,SIGNAL(finished()),this,SLOT(dictDataReady()));
}

void CSnTrans::selectionShow()
{
    if (storedSelection.isEmpty()) return;

    findWordTranslation(storedSelection);
}

void CSnTrans::hideTooltip()
{
    QxtToolTip::setToolTip(snv,NULL);
}

void CSnTrans::showWordTranslation(const QString &html)
{
    if (snv->ctxHandler->menuActive.isActive()) return;
    QSpecToolTipLabel *t = new QSpecToolTipLabel(html);
    t->setOpenExternalLinks(false);
    t->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    t->setMaximumSize(350,350);

    connect(t,SIGNAL(linkActivated(QString)),this,SLOT(showSuggestedTranslation(QString)));
    connect(t,SIGNAL(labelHide()),this,SLOT(hideTooltip()));
    connect(snv->ctxHandler,SIGNAL(hideTooltips()),t,SLOT(close()));

    QxtToolTip::show(QCursor::pos(),t,snv,QRect(),true);
}

void CSnTrans::showSuggestedTranslation(const QString &link)
{
    QUrl url(link);
    QUrlQuery requ(url);
    QString word = requ.queryItemValue("word");
    if (word.startsWith('%')) {
        QByteArray bword = word.toLatin1();
        if (!bword.isNull() && !bword.isEmpty())
            word = QUrl::fromPercentEncoding(bword);
    }
    findWordTranslation(word);
}

void CSnTrans::dictDataReady()
{
    QString res = QString();
    QNetworkReply* rep = qobject_cast<QNetworkReply *>(sender());
    if (rep!=NULL) {
        res = QString::fromUtf8(rep->readAll());
        rep->deleteLater();
    }
    showWordTranslation(res);
}
