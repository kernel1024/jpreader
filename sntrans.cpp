#include <QMessageBox>

#include <QUrlQuery>

#include "sntrans.h"
#include "globalcontrol.h"
#include "goldendict/goldendictmgr.h"
#include "translator.h"
#include "qxttooltip.h"

CSnTrans::CSnTrans(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;

    selectionTimer = new QTimer(this);
    selectionTimer->setInterval(1000);
    selectionTimer->setSingleShot(true);
    savedBaseUrl.clear();

    connect(snv->txtBrowser->page(), SIGNAL(selectionChanged()),this,SLOT(selectionChanged()));
    connect(selectionTimer, SIGNAL(timeout()),this,SLOT(selectionShow()));
}

void CSnTrans::reparseDocument()
{
    snv->txtBrowser->page()->toHtml([this](const QString& result) { reparseDocumentPriv(result); });
}

void CSnTrans::reparseDocumentPriv(const QString& data)
{
    snv->calculatedUrl="";
    snv->onceTranslated=true;
    QString aUri = data;
    savedBaseUrl = snv->txtBrowser->page()->url();
    if (savedBaseUrl.hasFragment())
        savedBaseUrl.setFragment(QString());

    CTranslator* ct = new CTranslator(NULL,aUri);
    QString res;
    if (!ct->documentReparse(aUri,res)) {
        QMessageBox::critical(snv,tr("JPReader"),tr("Translation to XML failed."));
        return;
    }

    snv->calculatedUrl=res;
    postTranslate();
    snv->parentWnd->updateTabs();
}

void CSnTrans::translate()
{
    if (gSet->translatorEngine==TE_ATLAS ||
        gSet->translatorEngine==TE_BINGAPI ||
        gSet->translatorEngine==TE_YANDEX) {
        savedBaseUrl = snv->txtBrowser->page()->url();
        if (savedBaseUrl.hasFragment())
            savedBaseUrl.setFragment(QString());
        snv->txtBrowser->page()->toHtml([this](const QString& html) {
            translatePriv(html);
        });
    } else {
        QString aUri = snv->txtBrowser->page()->url().toString();
        if (!aUri.isEmpty())
            translatePriv(aUri);
        else
            snv->txtBrowser->page()->toHtml([this](const QString& html) {
                translatePriv(html);
            });
    }
}

void CSnTrans::translatePriv(const QString &aUri)
{
    snv->calculatedUrl="";
    snv->onceTranslated=true;

    CTranslator* ct = new CTranslator(NULL,aUri);
    QThread* th = new QThread();
    connect(ct,SIGNAL(calcFinished(bool,QString)),
            this,SLOT(calcFinished(bool,QString)),Qt::QueuedConnection);
    snv->waitHandler->setProgressValue(0);
    snv->waitPanel->show();
    snv->transButton->setEnabled(false);
    if (gSet->translatorEngine==TE_ATLAS) {
        snv->waitHandler->setText(tr("Translating text with ATLAS..."));
        snv->waitHandler->setProgressEnabled(true);
    } else if (gSet->translatorEngine==TE_BINGAPI) {
        snv->waitHandler->setText(tr("Translating text with Bing API..."));
        snv->waitHandler->setProgressEnabled(true);
    } else if (gSet->translatorEngine==TE_YANDEX) {
        snv->waitHandler->setText(tr("Translating text with Yandex.Translate API..."));
        snv->waitHandler->setProgressEnabled(true);
    } else {
        snv->waitHandler->setText(tr("Copying file to hosting..."));
        snv->waitHandler->setProgressEnabled(false);
    }

    connect(gSet,SIGNAL(stopTranslators()),
            ct,SLOT(abortTranslator()),Qt::QueuedConnection);
    connect(snv->abortBtn,SIGNAL(clicked()),
            ct,SLOT(abortTranslator()),Qt::QueuedConnection);
    connect(ct,SIGNAL(setProgress(int)),
            snv->waitHandler,SLOT(setProgressValue(int)),Qt::QueuedConnection);

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
        if (aUrl.startsWith("ERROR:"))
            QMessageBox::warning(snv,tr("JPReader"),tr("Translator error.\n\n%1").arg(aUrl));
        else
            QMessageBox::warning(snv,tr("JPReader"),tr("Url not calculated. Network error occured."));
    }
}

void CSnTrans::postTranslate()
{
    if (snv->calculatedUrl.isEmpty()) return;
    if (snv->calculatedUrl.contains("ERROR:ATLAS_SLIPPED")) {
        QMessageBox::warning(snv,tr("JPReader"),tr("ATLAS slipped. Please restart translation."));
        return;
    }
    QUrl url;
    QString cn;
    QUrlQuery qu;
    switch (gSet->translatorEngine) {
        case TE_GOOGLE:
            url = QUrl("http://translate.google.com/translate");
            qu.addQueryItem("sl",gSet->getSourceLanguageID());
            qu.addQueryItem("tl","en");
            qu.addQueryItem("u",snv->calculatedUrl);
            url.setQuery(qu);
            snv->txtBrowser->load(url);
            if (snv->tabWidget->currentWidget()==snv) snv->txtBrowser->setFocus();
            break;
        case TE_ATLAS: // Url contains translated file itself
        case TE_BINGAPI:
        case TE_YANDEX:
            cn = snv->calculatedUrl;
            snv->fileChanged = true;
            snv->onceTranslated = true;
            snv->txtBrowser->setHtml(cn,savedBaseUrl);
            if (snv->tabWidget->currentWidget()==snv) snv->txtBrowser->setFocus();
            break;
        default:
            QMessageBox::warning(snv,tr("JPReader"),tr("Unknown translation engine selected"));
            return;
    }
    if (snv->parentWnd->tabMain->currentIndex()!=snv->parentWnd->tabMain->indexOf(snv) &&
            !snv->loadingBkgdFinished) { // tab is inactive
        snv->translationBkgdFinished=true;
        snv->parentWnd->tabMain->tabBar()->setTabTextColor(snv->parentWnd->tabMain->indexOf(snv),Qt::red);
        snv->parentWnd->updateTabs();
    }
}

void CSnTrans::progressLoad(int progress)
{
    snv->barLoading->setValue(progress);
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

void CSnTrans::showWordTranslation(const QString &html)
{
    if (snv->ctxHandler->menuActive.isActive()) return;
    CSpecToolTipLabel *t = new CSpecToolTipLabel(html);
    t->setOpenExternalLinks(false);
    t->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    t->setMaximumSize(350,350);
    t->setStyleSheet("QLabel { background: #fefdeb; }");

    connect(t,SIGNAL(linkActivated(QString)),this,SLOT(showSuggestedTranslation(QString)));
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
