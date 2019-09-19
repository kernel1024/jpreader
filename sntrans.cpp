#include <QMessageBox>
#include <QScopedPointer>
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

namespace CDefaults {
const int selectionTimerDelay = 1000;
const int longClickTimerDelay = 1000;
}

CSnTrans::CSnTrans(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;

    m_selectionTimer.setInterval(CDefaults::selectionTimerDelay);
    m_selectionTimer.setSingleShot(true);
    m_longClickTimer.setInterval(CDefaults::longClickTimerDelay);
    m_longClickTimer.setSingleShot(true);

    connect(snv->txtBrowser->page(), &QWebEnginePage::selectionChanged,this, &CSnTrans::selectionChanged);
    connect(&m_selectionTimer, &QTimer::timeout, this, &CSnTrans::selectionShow);
    connect(&m_longClickTimer, &QTimer::timeout, this, &CSnTrans::transButtonHighlight);
    connect(snv->transButton, &QPushButton::pressed, this, [this](){
        m_longClickTimer.start();
    });
    connect(snv->transButton, &QPushButton::released, this, [this](){
        bool ta = m_longClickTimer.isActive();
        if (ta) {
            m_longClickTimer.stop();
        } else {
            snv->transButton->setStyleSheet(QString());
        }
        translate(!ta);
    });
}

void CSnTrans::reparseDocument()
{
    snv->txtBrowser->page()->toHtml([this](const QString& result) { reparseDocumentPriv(result); });
}

void CSnTrans::reparseDocumentPriv(const QString& data)
{
    snv->m_translatedHtml.clear();
    snv->m_onceTranslated=true;
    m_savedBaseUrl = snv->txtBrowser->page()->url();
    if (m_savedBaseUrl.hasFragment())
        m_savedBaseUrl.setFragment(QString());

    QScopedPointer<CTranslator> ct(new CTranslator(nullptr,data));
    QString res;
    if (!ct->documentReparse(data,res)) {
        QMessageBox::critical(snv,tr("JPReader"),tr("Translation to XML failed."));
        return;
    }

    snv->m_translatedHtml=res;
    postTranslate();
    snv->parentWnd()->updateTabs();
}

void CSnTrans::transButtonHighlight()
{
    snv->transButton->setStyleSheet(QStringLiteral("QPushButton { background: #d7ffd7; }"));
}

void CSnTrans::translate(bool forceTranslateSubSentences)
{
    m_savedBaseUrl = snv->txtBrowser->page()->url();
    if (m_savedBaseUrl.hasFragment())
        m_savedBaseUrl.setFragment(QString());
    snv->txtBrowser->page()->toHtml([this,forceTranslateSubSentences](const QString& html) {
        translatePriv(html,forceTranslateSubSentences);
    });
}

void CSnTrans::getImgUrlsAndParse()
{
    snv->txtBrowser->page()->toHtml([this](const QString& result) {

        QScopedPointer<CTranslator> ct(new CTranslator(nullptr,result));
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

void CSnTrans::translatePriv(const QString &sourceHtml, bool forceTranslateSubSentences)
{
    snv->m_translatedHtml.clear();
    snv->m_onceTranslated=true;

    auto ct = new CTranslator(nullptr,sourceHtml,forceTranslateSubSentences);
    auto th = new QThread();
    connect(ct,&CTranslator::translationFinished,
            this,&CSnTrans::translationFinished,Qt::QueuedConnection);

    connect(ct,&CTranslator::finished,th,&QThread::quit);
    connect(th,&QThread::finished,ct,&CTranslator::deleteLater);
    connect(th,&QThread::finished,th,&QThread::deleteLater);

    snv->waitHandler->setProgressValue(0);
    snv->waitPanel->show();
    snv->transButton->setEnabled(false);
    snv->waitHandler->setProgressEnabled(true);

    const CLangPair lp = CLangPair(gSet->ui()->getActiveLangPair());
    const QChar arrow(0x2192);
    snv->waitHandler->setLanguage(QStringLiteral("%1 %2 %3")
                                  .arg(gSet->getLanguageName(lp.langFrom.bcp47Name()),
                                       arrow,
                                       gSet->getLanguageName(lp.langTo.bcp47Name())));
    snv->waitHandler->setText(tr("Translating text with %1")
                              .arg(CStructures::translationEngines().value(gSet->settings()->translatorEngine)));

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

void CSnTrans::translationFinished(bool success, const QString& resultHtml, const QString& error)
{
    snv->waitPanel->hide();
    snv->transButton->setEnabled(true);
    if (success) {
        snv->m_translatedHtml=resultHtml;
        postTranslate();
        snv->parentWnd()->updateTabs();
    } else {
        if (resultHtml.startsWith(QStringLiteral("ERROR:"))) {
            QMessageBox::warning(snv,tr("JPReader"),tr("Translator error.\n\n%1").arg(resultHtml));

        } else if (!error.isEmpty()) {
            QMessageBox::warning(snv,tr("JPReader"),tr("Translator error.\n\n%1").arg(error));

        } else {
            QMessageBox::warning(snv,tr("JPReader"),tr("Translation failed. Network error occured."));
        }
    }
}

void CSnTrans::postTranslate()
{
    if (snv->m_translatedHtml.isEmpty()) return;
    if (snv->m_translatedHtml.contains(QStringLiteral("ERROR:ATLAS_SLIPPED"))) {
        QMessageBox::warning(snv,tr("JPReader"),tr("ATLAS slipped. Please restart translation."));
        return;
    }

    QString cn = snv->m_translatedHtml;
    if (cn.toUtf8().size()<CDefaults::maxDataUrlFileSize) { // chromium dataurl 2Mb limitation, QTBUG-53414
        snv->m_fileChanged = true;
        snv->m_onceTranslated = true;
        snv->txtBrowser->setHtmlInterlocked(cn,m_savedBaseUrl);
        if (snv->tabWidget()->currentWidget()==snv) snv->txtBrowser->setFocus();
        snv->urlChanged(snv->netHandler->getLoadedUrl());
    } else
        openSeparateTranslationTab(cn, m_savedBaseUrl);

    snv->updateTabColor(false,true);
}

void CSnTrans::progressLoad(int progress)
{
    snv->barLoading->setValue(progress);
}

void CSnTrans::selectionChanged()
{
    m_storedSelection = snv->txtBrowser->page()->selectedText();
    if (!m_storedSelection.isEmpty() && gSet->ui()->actionSelectionDictionary->isChecked())
        m_selectionTimer.start();
}

void CSnTrans::findWordTranslation(const QString &text)
{
    QUrl req;
    req.setScheme( QStringLiteral("gdlookup") );
    req.setHost( QStringLiteral("localhost") );
    QUrlQuery requ;
    requ.addQueryItem( QStringLiteral("word"), text );
    req.setQuery(requ);
    QNetworkReply* rep = gSet->dictionaryNetworkAccessManager()->get(QNetworkRequest(req));
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
    if (m_storedSelection.isEmpty()) return;

    findWordTranslation(m_storedSelection);
}

void CSnTrans::showWordTranslation(const QString &html)
{
    const QSize maxTranslationTooltipSize(350,350);

    if (snv->ctxHandler->isMenuActive()) return;
    auto t = new QLabel(html);
    t->setOpenExternalLinks(false);
    t->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    t->setMaximumSize(maxTranslationTooltipSize);
    t->setStyleSheet(QStringLiteral("QLabel { background: #fefdeb; }"));

    connect(t,&QLabel::linkActivated,this,&CSnTrans::showSuggestedTranslation);
    connect(snv->ctxHandler,&CSnCtxHandler::hideTooltips,t,&QLabel::close);

    QxtToolTip::show(QCursor::pos(),t,snv,QRect(),true,true);
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
