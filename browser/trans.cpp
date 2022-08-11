#include <QMessageBox>
#include <QScopedPointer>
#include <QUrlQuery>
#include <QThread>

#include "trans.h"
#include "net.h"
#include "ctxhandler.h"
#include "waitctl.h"
#include "global/control.h"
#include "global/startup.h"
#include "global/browserfuncs.h"
#include "translator/translator.h"
#include "browser-utils/userscript.h"
#include "browser-utils/downloadmanager.h"
#include "miniqxt/qxttooltip.h"

namespace CDefaults {
const int selectionTimerDelay = 1000;
const int maxSuggestedWords = 6;
const int maxTooltipTranslationLength = 30;
const int maxTooltipSearchIterations = 15;
}

CBrowserTrans::CBrowserTrans(CBrowserTab *parent)
    : QObject(parent), snv(parent)
{
    m_selectionTimer.setInterval(CDefaults::selectionTimerDelay);
    m_selectionTimer.setSingleShot(true);

    connect(snv->txtBrowser->page(), &QWebEnginePage::selectionChanged,this, &CBrowserTrans::selectionChanged);
    connect(&m_selectionTimer, &QTimer::timeout, this, &CBrowserTrans::selectionShow);
    connect(snv->transButton, &QPushButton::clicked, this, &CBrowserTrans::translateDocument);
}

void CBrowserTrans::applyScripts()
{
    QUrl origin = snv->txtBrowser->page()->url();
    const QVector<CUserScript> scripts = gSet->browser()->getUserScriptsForUrl(origin,false,false,true);
    for (const auto &script : scripts) {
        QEventLoop ev;
        connect(this,&CBrowserTrans::scriptFinished,&ev,&QEventLoop::quit);
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

void CBrowserTrans::reparseDocument()
{
    applyScripts();
    snv->txtBrowser->page()->toHtml([this](const QString& result) { reparseDocumentPriv(result); });
}

void CBrowserTrans::reparseDocumentPriv(const QString& data)
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

void CBrowserTrans::translateDocument()
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

void CBrowserTrans::getUrlsFromPageAndParse()
{
    auto *nt = qobject_cast<QAction *>(sender());
    if (nt==nullptr) return;
    const auto mode = static_cast<UrlsExtractorMode>(nt->data().toInt());

    snv->txtBrowser->page()->toHtml([this,mode](const QString& result) {

        QScopedPointer<CTranslator> ct(new CTranslator(nullptr,result));
        QString res;
        if (!ct->documentReparse(result,res)) {
            QMessageBox::critical(snv,QGuiApplication::applicationDisplayName(),
                                  tr("Parsing failed. Unable to get urls."));
            return;
        }

        QUrl baseUrl = snv->txtBrowser->page()->url();
        if (baseUrl.hasFragment())
            baseUrl.setFragment(QString());
        QVector<CUrlWithName> urls;
        QStringList sl;
        if (mode == uemAllFiles) {
            sl = ct->getAnchorUrls();
        } else if (mode == uemImages) {
            sl = ct->getImgUrls();
        }
        urls.reserve(sl.count());
        for (const QString& s : qAsConst(sl)) {
            QUrl u = QUrl(s);
            if (u.isRelative())
                u = baseUrl.resolved(u);
            if (mode == uemAllFiles) {
                if (u.fileName().isEmpty())
                    continue;
            }
            urls.append(qMakePair(u.toString(),computeFileName(u)));
        }
        gSet->downloadManager()->multiFileDownload(urls, baseUrl, baseUrl.fileName(), false, false);
    });
}

QString CBrowserTrans::computeFileName(const QUrl &url) const
{
    if (url.host().endsWith(QSL("kemono.party"),Qt::CaseInsensitive) &&
            url.path().startsWith(QSL("/data/"))) {
        const QUrlQuery uq(url);
        return uq.queryItemValue(QSL("f"),QUrl::FullyDecoded);
    }

    return QString();
}

void CBrowserTrans::translatePriv(const QString &sourceHtml, const QString &title, const QUrl &origin)
{
    snv->m_translatedHtml.clear();
    snv->m_onceTranslated=true;
    snv->m_waitHandler->setProgressValue(0);
    snv->waitPanel->show();
    snv->transButton->setEnabled(false);
    snv->m_waitHandler->setProgressEnabled(true);

    CStructures::TranslationEngine engine = gSet->settings()->translatorEngine;
    if (snv->m_requestAlternateAutotranslate)
        engine = gSet->settings()->previousTranslatorEngine;
    CLangPair lp(gSet->settings()->getSelectedLangPair(engine));

    snv->m_waitHandler->setLanguage(lp.toLongString());
    snv->m_waitHandler->setText(tr("Translating text with %1")
                              .arg(CStructures::translationEngines().value(engine)));

    auto *ct = new CTranslator(nullptr,sourceHtml,title,origin,engine,lp);
    if (!gSet->startup()->setupThreadedWorker(ct)) {
        delete ct;
        return;
    }

    connect(ct,&CTranslator::translationFinished,
            this,&CBrowserTrans::translationFinished,Qt::QueuedConnection);
    connect(snv->abortBtn,&QPushButton::clicked,
            ct,&CAbstractThreadWorker::abort,Qt::QueuedConnection);
    connect(ct,&CTranslator::setProgress,
            snv->m_waitHandler,&CBrowserWaitCtl::setProgressValue,Qt::QueuedConnection);

    QMetaObject::invokeMethod(ct,&CAbstractThreadWorker::start,Qt::QueuedConnection);
}

void CBrowserTrans::translationFinished(bool success, bool aborted, const QString& resultHtml, const QString& error)
{
    Q_UNUSED(aborted)

    snv->waitPanel->hide();
    snv->transButton->setEnabled(true);
    if (!resultHtml.isEmpty() && !resultHtml.startsWith(QSL("ERROR:"))) {
        snv->m_translatedHtml=resultHtml;
        postTranslate();
        snv->parentWnd()->updateTabs();
    }

    if (!success) {
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

void CBrowserTrans::postTranslate()
{
    if (snv->m_translatedHtml.isEmpty()) return;
    if (snv->m_translatedHtml.contains(QSL("ERROR:ATLAS_SLIPPED"))) {
        QMessageBox::warning(snv,QGuiApplication::applicationDisplayName(),
                             tr("ATLAS slipped. Please restart translation."));
        return;
    }

    QString cn = snv->m_translatedHtml;
    snv->m_netHandler->load(cn,m_savedBaseUrl,false,false,true);
    snv->updateTabColor(false,true);
}

void CBrowserTrans::selectionChanged()
{
    m_storedSelection = snv->txtBrowser->page()->selectedText();
    if (!m_storedSelection.isEmpty() && gSet->actions()->actionSelectionDictionary->isChecked())
        m_selectionTimer.start();
}

void CBrowserTrans::findWordTranslation(const QString &text)
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
    th->setObjectName(QSL("SNV_wordLookup"));
    th->start();
}

void CBrowserTrans::selectionShow()
{
    if (m_storedSelection.isEmpty()) return;

    findWordTranslation(m_storedSelection);
}

void CBrowserTrans::showWordTranslation(const QString &html)
{
    const QSize maxTranslationTooltipSize(350,350);

    if (snv->m_ctxHandler->isMenuActive()) return;
    auto *t = new QLabel(html);
    t->setOpenExternalLinks(false);
    t->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    t->setMaximumSize(maxTranslationTooltipSize);
    t->setStyleSheet(QSL("QLabel { background: #fefdeb; }"));

    connect(t,&QLabel::linkActivated,this,&CBrowserTrans::showSuggestedTranslation);
    connect(snv->m_ctxHandler,&CBrowserCtxHandler::hideTooltips,t,&QLabel::close);

    QxtToolTip::show(QCursor::pos(),t,snv,QRect(),true,true);
}

void CBrowserTrans::showSuggestedTranslation(const QString &link)
{
    QUrl url(link);
    QUrlQuery requ(url);
    QString word = requ.queryItemValue(QSL("word"));
    if (word.startsWith(u'%')) {
        QByteArray bword = word.toLatin1();
        if (!bword.isNull() && !bword.isEmpty())
            word = QUrl::fromPercentEncoding(bword);
    }
    findWordTranslation(word);
}
