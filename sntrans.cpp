#include <QMessageBox>
#include <QScopedPointer>
#include <QUrlQuery>
#include <QThread>

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
const int maxSuggestedWords = 6;
const int maxTooltipTranslationLength = 30;
const int maxTooltipSearchIterations = 15;
}

CSnTrans::CSnTrans(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;

    m_selectionTimer.setInterval(CDefaults::selectionTimerDelay);
    m_selectionTimer.setSingleShot(true);

    connect(snv->txtBrowser->page(), &QWebEnginePage::selectionChanged,this, &CSnTrans::selectionChanged);
    connect(&m_selectionTimer, &QTimer::timeout, this, &CSnTrans::selectionShow);
    connect(snv->transButton, &QPushButton::clicked, this, &CSnTrans::translateDocument);
}

void CSnTrans::applyScripts()
{
    QUrl origin = snv->txtBrowser->page()->url();
    const QVector<CUserScript> scripts = gSet->getUserScriptsForUrl(origin,false,false,true);
    for (const auto &script : scripts) {
        QEventLoop ev;
        connect(this,&CSnTrans::scriptFinished,&ev,&QEventLoop::quit);
        QMetaObject::invokeMethod(this,[this,script](){
            snv->txtBrowser->page()->runJavaScript(script.getSource(),
                                                   QWebEngineScript::MainWorld,[this](const QVariant &v){
                Q_UNUSED(v)
                Q_EMIT scriptFinished();
            });
        },Qt::QueuedConnection);
        ev.exec();
    }
}

void CSnTrans::reparseDocument()
{
    applyScripts();
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
        QMessageBox::critical(snv,QGuiApplication::applicationDisplayName(),
                              tr("Parsing failed."));
        return;
    }

    snv->m_translatedHtml=res;
    postTranslate();
    snv->parentWnd()->updateTabs();
}

void CSnTrans::translateDocument()
{
    m_savedBaseUrl = snv->txtBrowser->page()->url();
    if (m_savedBaseUrl.hasFragment())
        m_savedBaseUrl.setFragment(QString());
    applyScripts();
    const QString title = snv->txtBrowser->title();
    snv->txtBrowser->page()->toHtml([this,title](const QString& html) {
        translatePriv(html,title,m_savedBaseUrl);
    });
}

void CSnTrans::getImgUrlsAndParse()
{
    snv->txtBrowser->page()->toHtml([this](const QString& result) {

        QScopedPointer<CTranslator> ct(new CTranslator(nullptr,result));
        QString res;
        if (!ct->documentReparse(result,res)) {
            QMessageBox::critical(snv,QGuiApplication::applicationDisplayName(),
                                  tr("Parsing failed. Unable to get image urls."));
            return;
        }

        QUrl baseUrl = snv->txtBrowser->page()->url();
        if (baseUrl.hasFragment())
            baseUrl.setFragment(QString());
        QVector<CUrlWithName> urls;
        const QStringList sl = ct->getImgUrls();
        urls.reserve(sl.count());
        for (const QString& s : sl) {
            QUrl u = QUrl(s);
            if (u.isRelative())
                u = baseUrl.resolved(u);
            urls.append(qMakePair(u.toString(),QString()));
        }
        snv->netHandler->multiImgDownload(urls, baseUrl);
    });
}

void CSnTrans::translatePriv(const QString &sourceHtml, const QString &title, const QUrl &origin)
{
    snv->m_translatedHtml.clear();
    snv->m_onceTranslated=true;
    snv->waitHandler->setProgressValue(0);
    snv->waitPanel->show();
    snv->transButton->setEnabled(false);
    snv->waitHandler->setProgressEnabled(true);

    snv->waitHandler->setLanguage(CLangPair(gSet->ui()->getActiveLangPair()).toShortString());
    snv->waitHandler->setText(tr("Translating text with %1")
                              .arg(CStructures::translationEngines().value(gSet->settings()->translatorEngine)));

    auto *ct = new CTranslator(nullptr,sourceHtml,title,origin);
    gSet->setupThreadedWorker(ct);

    connect(ct,&CTranslator::translationFinished,
            this,&CSnTrans::translationFinished,Qt::QueuedConnection);
    connect(snv->abortBtn,&QPushButton::clicked,
            ct,&CAbstractThreadWorker::abort,Qt::QueuedConnection);
    connect(ct,&CTranslator::setProgress,
            snv->waitHandler,&CSnWaitCtl::setProgressValue,Qt::QueuedConnection);

    QMetaObject::invokeMethod(ct,&CAbstractThreadWorker::start,Qt::QueuedConnection);
}

void CSnTrans::translationFinished(bool success, bool aborted, const QString& resultHtml, const QString& error)
{
    snv->waitPanel->hide();
    snv->transButton->setEnabled(true);
    if (aborted) return;

    if (success) {
        snv->m_translatedHtml=resultHtml;
        postTranslate();
        snv->parentWnd()->updateTabs();
    } else {
        if (resultHtml.startsWith(QSL("ERROR:"))) {
            QMessageBox::warning(snv,QGuiApplication::applicationDisplayName(),
                                 tr("Translator error.\n\n%1").arg(resultHtml));

        } else if (!error.isEmpty()) {
            QMessageBox::warning(snv,QGuiApplication::applicationDisplayName(),
                                 tr("Translator error.\n\n%1").arg(error));

        } else {
            QMessageBox::warning(snv,QGuiApplication::applicationDisplayName(),
                                 tr("Translation failed. Network error occured."));
        }
    }
}

void CSnTrans::postTranslate()
{
    if (snv->m_translatedHtml.isEmpty()) return;
    if (snv->m_translatedHtml.contains(QSL("ERROR:ATLAS_SLIPPED"))) {
        QMessageBox::warning(snv,QGuiApplication::applicationDisplayName(),
                             tr("ATLAS slipped. Please restart translation."));
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

void CSnTrans::selectionChanged()
{
    m_storedSelection = snv->txtBrowser->page()->selectedText();
    if (!m_storedSelection.isEmpty() && gSet->ui()->actionSelectionDictionary->isChecked())
        m_selectionTimer.start();
}

void CSnTrans::findWordTranslation(const QString &text)
{
    if (text.isEmpty() || (text.length()>CDefaults::maxTooltipTranslationLength)) return;

    QThread *th = QThread::create([this,text]{
        QString searchWord = text;
        QStringList words = gSet->dictionaryManager()->wordLookup(searchWord);
        int cnt = 0;
        while (words.isEmpty() && !searchWord.isEmpty() && (cnt<CDefaults::maxTooltipSearchIterations)) {
            searchWord.truncate(searchWord.length()-1);
            words = gSet->dictionaryManager()->wordLookup(searchWord,true);
            cnt++;
        }

        QString article;
        if (!words.isEmpty()) {
            if (searchWord == text) {
                article = gSet->dictionaryManager()->loadArticle(words.first()); // word found
            } else { // word is too long, similar words found
                article = QSL("<div><b>Not found</b><br/>");
                for (int i=0; i<qMin(words.count(),CDefaults::maxSuggestedWords); i++) {
                    if (i>0)
                        article += QSL(", ");
                    article += QSL("<a href=\"zdict?word=%1\">%2</a>")
                               .arg(QUrl::toPercentEncoding(words.at(i)),words.at(i).toHtmlEscaped());
                }
                article += QSL("</div>");
            }
        } else {
            article = QSL("<div><b>Not found</b></div>");
        }
        if (!article.isEmpty()) {
            QMetaObject::invokeMethod(this,[this,article](){
                showWordTranslation(article);
            },Qt::QueuedConnection);
        }
    });

    connect(th,&QThread::finished,th,&QThread::deleteLater);
    th->start();
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
    auto *t = new QLabel(html);
    t->setOpenExternalLinks(false);
    t->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    t->setMaximumSize(maxTranslationTooltipSize);
    t->setStyleSheet(QSL("QLabel { background: #fefdeb; }"));

    connect(t,&QLabel::linkActivated,this,&CSnTrans::showSuggestedTranslation);
    connect(snv->ctxHandler,&CSnCtxHandler::hideTooltips,t,&QLabel::close);

    QxtToolTip::show(QCursor::pos(),t,snv,QRect(),true,true);
}

void CSnTrans::showSuggestedTranslation(const QString &link)
{
    QUrl url(link);
    QUrlQuery requ(url);
    QString word = requ.queryItemValue(QSL("word"));
    if (word.startsWith('%')) {
        QByteArray bword = word.toLatin1();
        if (!bword.isNull() && !bword.isEmpty())
            word = QUrl::fromPercentEncoding(bword);
    }
    findWordTranslation(word);
}
