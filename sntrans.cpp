#include <QMessageBox>
#include <QUrlQuery>
#include <goldendictlib/goldendictmgr.hh>

#include "sntrans.h"
#include "snnet.h"
#include "snctxhandler.h"
#include "globalcontrol.h"
#include "translator.h"
#include "qxttooltip.h"
#include "genericfuncs.h"

using namespace htmlcxx;

CSnTrans::CSnTrans(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;

    selectionTimer = new QTimer(this);
    selectionTimer->setInterval(1000);
    selectionTimer->setSingleShot(true);
    longClickTimer = new QTimer(this);
    longClickTimer->setInterval(1000);
    longClickTimer->setSingleShot(true);
    savedBaseUrl.clear();

    connect(snv->txtBrowser->page(), &QWebEnginePage::selectionChanged,this, &CSnTrans::selectionChanged);
    connect(selectionTimer, &QTimer::timeout, this, &CSnTrans::selectionShow);
    connect(longClickTimer, &QTimer::timeout, this, &CSnTrans::transButtonHighlight);
    connect(snv->transButton, &QPushButton::pressed, this, [this](){
        longClickTimer->start();
    });
    connect(snv->transButton, &QPushButton::released, this, [this](){
        bool ta = longClickTimer->isActive();
        if (ta)
            longClickTimer->stop();
        else
            snv->transButton->setStyleSheet(QString());
        translate(!ta);
    });
}

void CSnTrans::reparseDocument()
{
    snv->txtBrowser->page()->toHtml([this](const QString& result) { reparseDocumentPriv(result); });
}

void CSnTrans::reparseDocumentPriv(const QString& data)
{
    snv->calculatedUrl.clear();
    snv->onceTranslated=true;
    savedBaseUrl = snv->txtBrowser->page()->url();
    if (savedBaseUrl.hasFragment())
        savedBaseUrl.setFragment(QString());

    auto ct = new CTranslator(nullptr,data);
    QString res;
    if (!ct->documentReparse(data,res)) {
        QMessageBox::critical(snv,tr("JPReader"),tr("Translation to XML failed."));
        return;
    }

    snv->calculatedUrl=res;
    postTranslate();
    snv->parentWnd()->updateTabs();
}

void CSnTrans::transButtonHighlight()
{
    snv->transButton->setStyleSheet(QStringLiteral("QPushButton { background: #d7ffd7; }"));
}

void CSnTrans::translate(bool tranSubSentences)
{
    if (gSet->settings.translatorEngine==teAtlas ||
        gSet->settings.translatorEngine==teBingAPI ||
        gSet->settings.translatorEngine==teYandexAPI ||
        gSet->settings.translatorEngine==teGoogleGTX) {
        savedBaseUrl = snv->txtBrowser->page()->url();
        if (savedBaseUrl.hasFragment())
            savedBaseUrl.setFragment(QString());
        snv->txtBrowser->page()->toHtml([this,tranSubSentences](const QString& html) {
            translatePriv(html,tranSubSentences);
        });
    } else {
        QString aUri = snv->txtBrowser->page()->url().toString();
        if (!aUri.isEmpty())
            translatePriv(aUri,tranSubSentences);
        else
            snv->txtBrowser->page()->toHtml([this,tranSubSentences](const QString& html) {
                translatePriv(html,tranSubSentences);
            });
    }
}

void CSnTrans::getImgUrlsAndParse()
{
    snv->txtBrowser->page()->toHtml([this](const QString& result) {

        auto ct = new CTranslator(nullptr,result);
        QString res;
        if (!ct->documentReparse(result,res)) {
            QMessageBox::critical(snv,tr("JPReader"),
                                  tr("Translation to XML failed. Unable to get image urls."));
            return;
        }

        QUrl baseUrl = snv->txtBrowser->page()->url();
        if (baseUrl.hasFragment())
            baseUrl.setFragment(QString());
        QStringList urls;
        const QStringList sl = ct->getImgUrls();
        urls.reserve(sl.count());
        for (const QString& s : sl) {
            QUrl u = QUrl(s);
            if (u.isRelative())
                u = baseUrl.resolved(u);
            urls << u.toString();
        }
        snv->netHandler->multiImgDownload(urls, baseUrl);
    });
}

void CSnTrans::translatePriv(const QString &aUri, bool forceTranSubSentences)
{
    snv->calculatedUrl.clear();
    snv->onceTranslated=true;

    auto ct = new CTranslator(nullptr,aUri,forceTranSubSentences);
    auto th = new QThread();
    connect(ct,&CTranslator::calcFinished,
            this,&CSnTrans::calcFinished,Qt::QueuedConnection);
    snv->waitHandler->setProgressValue(0);
    snv->waitPanel->show();
    snv->transButton->setEnabled(false);
    if (gSet->settings.translatorEngine==teAtlas) {
        snv->waitHandler->setText(tr("Translating text with ATLAS..."));
        snv->waitHandler->setProgressEnabled(true);
    } else if (gSet->settings.translatorEngine==teBingAPI) {
        snv->waitHandler->setText(tr("Translating text with Bing API..."));
        snv->waitHandler->setProgressEnabled(true);
    } else if (gSet->settings.translatorEngine==teYandexAPI) {
        snv->waitHandler->setText(tr("Translating text with Yandex.Translate API..."));
        snv->waitHandler->setProgressEnabled(true);
    } else if (gSet->settings.translatorEngine==teGoogleGTX) {
        snv->waitHandler->setText(tr("Translating text with Google GTX..."));
        snv->waitHandler->setProgressEnabled(true);
    } else {
        snv->waitHandler->setText(tr("Copying file to hosting..."));
        snv->waitHandler->setProgressEnabled(false);
    }

    connect(gSet,&CGlobalControl::stopTranslators,
            ct,&CTranslator::abortTranslator,Qt::QueuedConnection);
    connect(snv->abortBtn,&QPushButton::clicked,
            ct,&CTranslator::abortTranslator,Qt::QueuedConnection);
    connect(ct,&CTranslator::setProgress,
            snv->waitHandler,&CSnWaitCtl::setProgressValue,Qt::QueuedConnection);

    ct->moveToThread(th);
    th->start();

    QMetaObject::invokeMethod(ct,&CTranslator::translate,Qt::QueuedConnection);
}

void CSnTrans::calcFinished(bool success, const QString& aUrl, const QString& error)
{
    snv->waitPanel->hide();
    snv->transButton->setEnabled(true);
    if (success) {
        snv->calculatedUrl=aUrl;
        postTranslate();
        snv->parentWnd()->updateTabs();
    } else {
        if (aUrl.startsWith(QStringLiteral("ERROR:")))
            QMessageBox::warning(snv,tr("JPReader"),tr("Translator error.\n\n%1").arg(aUrl));
        else if (!error.isEmpty())
            QMessageBox::warning(snv,tr("JPReader"),tr("Translator error.\n\n%1").arg(error));
        else
            QMessageBox::warning(snv,tr("JPReader"),tr("Translation failed. Network error occured."));
    }
}

void CSnTrans::postTranslate()
{
    if (snv->calculatedUrl.isEmpty()) return;
    if (snv->calculatedUrl.contains(QStringLiteral("ERROR:ATLAS_SLIPPED"))) {
        QMessageBox::warning(snv,tr("JPReader"),tr("ATLAS slipped. Please restart translation."));
        return;
    }
    QUrl url;
    QString cn;
    QUrlQuery qu;
    CLangPair lp;
    switch (gSet->settings.translatorEngine) {
        case teGoogle:
            url = QUrl(QStringLiteral("http://translate.google.com/translate"));
            lp = CLangPair(gSet->ui.getActiveLangPair());
            if (lp.isValid()) {
                qu.addQueryItem(QStringLiteral("sl"),lp.langFrom.bcp47Name());
                qu.addQueryItem(QStringLiteral("tl"),lp.langTo.bcp47Name());
                qu.addQueryItem(QStringLiteral("u"),snv->calculatedUrl);
                url.setQuery(qu);
                snv->txtBrowser->load(url);
                if (snv->tabWidget()->currentWidget()==snv) snv->txtBrowser->setFocus();
            } else
                QMessageBox::warning(snv,tr("JPReader"),tr("No active language pair selected"));
            break;
        case teAtlas: // Url contains translated file itself
        case teBingAPI:
        case teYandexAPI:
        case teGoogleGTX:
            cn = snv->calculatedUrl;
            if (cn.toUtf8().size()<1024*1024) { // chromium dataurl 2Mb limitation, QTBUG-53414
                snv->fileChanged = true;
                snv->onceTranslated = true;
                snv->txtBrowser->setHtml(cn,savedBaseUrl);
                if (snv->tabWidget()->currentWidget()==snv) snv->txtBrowser->setFocus();
                snv->urlChanged(snv->netHandler->loadedUrl);
            } else
                openSeparateTranslationTab(cn, savedBaseUrl);
            break;
        default:
            QMessageBox::warning(snv,tr("JPReader"),tr("Unknown translation engine selected"));
            return;
    }
    // TODO: move this to specwidgets
    if (snv->parentWnd()->tabMain->currentIndex()!=snv->parentWnd()->tabMain->indexOf(snv) &&
            !snv->loadingBkgdFinished) { // tab is inactive
        snv->translationBkgdFinished=true;
        if (snv->fileChanged) {
            snv->parentWnd()->tabMain->tabBar()->setTabTextColor(snv->parentWnd()->tabMain->indexOf(snv),Qt::red);
            snv->parentWnd()->updateTabs();
        }
    }
}

void CSnTrans::progressLoad(int progress)
{
    snv->barLoading->setValue(progress);
}

void CSnTrans::selectionChanged()
{
    storedSelection = snv->txtBrowser->page()->selectedText();
    if (!storedSelection.isEmpty() && gSet->ui.actionSelectionDictionary->isChecked())
        selectionTimer->start();
}

void CSnTrans::findWordTranslation(const QString &text)
{
    QUrl req;
    req.setScheme( QStringLiteral("gdlookup") );
    req.setHost( QStringLiteral("localhost") );
    QUrlQuery requ;
    requ.addQueryItem( QStringLiteral("word"), text );
    req.setQuery(requ);
    QNetworkReply* rep = gSet->dictNetMan->get(QNetworkRequest(req));
    connect(rep,&QNetworkReply::finished,this,&CSnTrans::dictDataReady);
}

void CSnTrans::openSeparateTranslationTab(const QString &html, const QUrl& baseUrl)
{
    HTML::ParserDom parser;
    parser.parse(html);
    tree<HTML::Node> tree = parser.getTree();
    CHTMLNode doc(tree);
    QString dst;
    CTranslator::replaceLocalHrefs(doc, baseUrl);
    CTranslator::generateHTML(doc,dst);

    snv->netHandler->loadWithTempFile(dst, true);
}

void CSnTrans::selectionShow()
{
    if (storedSelection.isEmpty()) return;

    findWordTranslation(storedSelection);
}

void CSnTrans::showWordTranslation(const QString &html)
{
    if (snv->ctxHandler->menuActive->isActive()) return;
    auto t = new CSpecToolTipLabel(html);
    t->setOpenExternalLinks(false);
    t->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    t->setMaximumSize(350,350);
    t->setStyleSheet(QStringLiteral("QLabel { background: #fefdeb; }"));

    connect(t,&CSpecToolTipLabel::linkActivated,this,&CSnTrans::showSuggestedTranslation);
    connect(snv->ctxHandler,&CSnCtxHandler::hideTooltips,t,&CSpecToolTipLabel::close);

    QxtToolTip::show(QCursor::pos(),t,snv,QRect(),true);
}

void CSnTrans::showSuggestedTranslation(const QString &link)
{
    QUrl url(link);
    QUrlQuery requ(url);
    QString word = requ.queryItemValue(QStringLiteral("word"));
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
    auto rep = qobject_cast<QNetworkReply *>(sender());
    if (rep) {
        res = QString::fromUtf8(rep->readAll());
        rep->deleteLater();
    }
    showWordTranslation(res);
}
