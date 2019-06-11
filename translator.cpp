#include <QProcess>
#include <QThread>
#include <QBuffer>
#include "translator.h"
#include "snviewer.h"
#include "genericfuncs.h"
#include <sstream>

using namespace htmlcxx;

CTranslator::CTranslator(QObject* parent, const QString& sourceHtml, bool forceTranSubSentences)
    : QObject(parent)
{
    m_sourceHtml=sourceHtml;
    m_translationEngine=gSet->settings()->translatorEngine;
    m_atlTcpRetryCount=gSet->settings()->atlTcpRetryCount;
    m_atlTcpTimeout=gSet->settings()->atlTcpTimeout;
    m_forceFontColor=gSet->ui()->forceFontColor();
    m_forcedFontColor=gSet->settings()->forcedFontColor;
    m_useOverrideFont=gSet->ui()->useOverrideFont();
    m_overrideFont=gSet->settings()->overrideFont;
    m_translationMode=gSet->ui()->getTranslationMode();
    m_translateSubSentences=(forceTranSubSentences || gSet->ui()->translateSubSentences());
}

CTranslator::~CTranslator()
{
    if (m_tran)
        m_tran->deleteLater();
}

bool CTranslator::translateDocument(const QString &srcHtml, QString &dstHtml)
{
    m_abortMutex.lock();
    m_abortFlag=false;
    m_abortMutex.unlock();

    if (m_tran==nullptr && !m_tranInited) {
        m_tran = translatorFactory(this, CLangPair(gSet->ui()->getActiveLangPair()));
    }

    if (m_tran==nullptr || !m_tran->initTran()) {
        dstHtml=tr("Unable to initialize translation engine.");
        qCritical() << tr("Unable to initialize translation engine.");
        return false;
    }
    m_tranInited = true;

    dstHtml.clear();
    if (srcHtml.isEmpty()) return false;

    QUuid token = QUuid::createUuid();

    QString src = srcHtml.trimmed();
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("1-source"),src);
    src = src.remove(0,src.indexOf(QStringLiteral("<html"),Qt::CaseInsensitive));

    HTML::ParserDom parser;
    parser.parse(src);

    tree<HTML::Node> tr = parser.getTree();

    if (gSet->settings()->debugDumpHtml) {
        std::stringstream sst;
        sst << tr;
        dumpPage(token,QStringLiteral("2-tree"),QByteArray::fromRawData(sst.str().data(),
                                                        static_cast<int>(sst.str().size())));
    }

    CHTMLNode doc(tr);

    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("3-converted"),doc);

    m_translatorFailed = false;
    m_textNodesCnt=0;
    m_metaSrcUrl.clear();
    examineNode(doc,PXPreprocess);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("4-preprocessed"),doc);

    examineNode(doc,PXCalculate);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("5-calculated"),doc);

    m_textNodesProgress=0;
    examineNode(doc,PXTranslate);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("6-translated"),doc);

    examineNode(doc,PXPostprocess);
    generateHTML(doc,dstHtml);

    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("7-finalized"),dstHtml);

    m_tran->doneTran();

    return !m_translatorFailed;
}

bool CTranslator::documentReparse(const QString &sourceHtml, QString &destHtml)
{
    m_abortMutex.lock();
    m_abortFlag=false;
    m_abortMutex.unlock();

    destHtml.clear();
    if (sourceHtml.isEmpty()) return false;

    QUuid token = QUuid::createUuid();

    QString src = sourceHtml.trimmed();
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-1-source"),src);
    src = src.remove(0,src.indexOf(QStringLiteral("<html"),Qt::CaseInsensitive));

    HTML::ParserDom parser;
    parser.parse(src);

    tree<HTML::Node> tr = parser.getTree();

    if (gSet->settings()->debugDumpHtml) {
        std::stringstream sst;
        sst << tr;
        dumpPage(token,QStringLiteral("parser-2-tree"),QByteArray::fromRawData(sst.str().data(),
                                                               static_cast<int>(sst.str().size())));
    }

    CHTMLNode doc(tr);

    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-3-converted"),doc);

    m_translatorFailed=false;
    m_textNodesCnt=0;
    m_metaSrcUrl.clear();
    examineNode(doc,PXPreprocess);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-4-preprocessed"),doc);

    examineNode(doc,PXCalculate);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-5-calculated"),doc);

    m_textNodesProgress=0;
    examineNode(doc,PXTranslate);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-6-translated"),doc);

    generateHTML(doc,destHtml);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-7-finalized"),destHtml);

    return true;
}

QStringList CTranslator::getImgUrls() const
{
    return m_imgUrls;
}

void CTranslator::examineNode(CHTMLNode &node, CTranslator::XMLPassMode xmlPass)
{
    if (m_translatorFailed) return;
    m_abortMutex.lock();
    if (m_abortFlag) {
        m_abortMutex.unlock();
        return;
    }
    m_abortMutex.unlock();

    QApplication::processEvents();

    static const QStringList skipTags { QStringLiteral("style"), QStringLiteral("title") };
    static const QStringList unhide_classes { QStringLiteral("novel-text-wrapper"),
                QStringLiteral("novel-pages"), QStringLiteral("novel-page") };
    static const QStringList denyTags { QStringLiteral("script"), QStringLiteral("noscript"),
                QStringLiteral("object"), QStringLiteral("iframe") };
    static const QStringList denyDivs { QStringLiteral("sh_fc2blogheadbar"),
                QStringLiteral("fc2_bottom_bnr"), QStringLiteral("global-header") };

    const QStringList &acceptedExt = CGenericFuncs::getSupportedImageExtensions();

    QVector<CHTMLNode> subnodes;

    if ((xmlPass==PXTranslate || xmlPass==PXCalculate) && m_tranInited) {
        if (skipTags.contains(node.tagName,Qt::CaseInsensitive)) return;

        if (xmlPass==PXCalculate && !node.isTag && !node.isComment) {
            translateParagraph(node,xmlPass);
        }
        if (xmlPass==PXTranslate && !node.isTag && !node.isComment) {
            if (!translateParagraph(node,xmlPass)) m_translatorFailed=true;
        }
    }

    if (xmlPass==PXPreprocess) {
        // WebEngine gives UTF-8 encoded page
        if (node.tagName.toLower()==QStringLiteral("meta")) {
            if (node.attributes.value(QStringLiteral("http-equiv")).toLower().trimmed()==QStringLiteral("content-type"))
                node.attributes[QStringLiteral("content")] = QStringLiteral("text/html; charset=UTF-8");
        }

        // Unfold "1% height" divs from blog.jp/livedoor.jp
        // their blogs using same site.css
        // Also for blog.goo.ne.jp (same CSS as static.css)
        int idx = 0;
        while (idx<node.children.count()) {
            if (node.children.at(idx).tagName.toLower()==QStringLiteral("meta")) {
                if (node.children.at(idx).attributes.value(QStringLiteral("property"))
                        .toLower().trimmed()==QStringLiteral("og:url"))
                    m_metaSrcUrl = QUrl(node.children.at(idx).attributes.value(QStringLiteral("content")).toLower().trimmed());
            }

            if (node.children.at(idx).tagName.toLower()==QStringLiteral("link")) {
                if (node.children.at(idx).attributes.value(QStringLiteral("rel"))
                        .toLower().trimmed()==QStringLiteral("canonical"))
                    m_metaSrcUrl = QUrl(node.children.at(idx).attributes.value(QStringLiteral("href")).toLower().trimmed());

                if ((node.children.at(idx).attributes.value(QStringLiteral("type"))
                     .toLower().trimmed()==QStringLiteral("text/css")) &&
                        (node.children.at(idx).attributes.value(QStringLiteral("href")).contains(
                             QRegExp(QStringLiteral("blog.*\\.jp.*site.css\\?_="),Qt::CaseInsensitive)) ||
                         (node.children.at(idx).attributes.value(QStringLiteral("href")).contains(
                             QRegExp(QStringLiteral("/css/.*static.css\\?"),Qt::CaseInsensitive)) &&
                          m_metaSrcUrl.host().contains(QRegExp(QStringLiteral("blog"),Qt::CaseInsensitive))))) {
                    CHTMLNode st;
                    st.tagName=QStringLiteral("style");
                    st.closingText=QStringLiteral("</style>");
                    st.isTag=true;
                    st.children << CHTMLNode(QStringLiteral("* { height: auto !important; }"));
                    node.children.insert(idx+1,st);
                    break;
                }
            }
            idx++;
        }

        // div processing
        if (node.tagName.toLower()==QStringLiteral("div")) {

            // unfold compressed divs
            QString sdivst;
            if (node.attributes.contains(QStringLiteral("style")))
                sdivst = node.attributes.value(QStringLiteral("style"));
            if (!sdivst.contains(QStringLiteral("absolute"),Qt::CaseInsensitive)) {
                if (!node.attributes.contains(QStringLiteral("style"))) {
                    node.attributes[QStringLiteral("style")]=QString();
                }
                if (node.attributes.value(QStringLiteral("style")).isEmpty()) {
                    node.attributes[QStringLiteral("style")]=QStringLiteral("height:auto;");
                } else {
                    node.attributes[QStringLiteral("style")]=
                            node.attributes.value(QStringLiteral("style"))
                            +QStringLiteral("; height:auto;");
                }
            }

            // pixiv - unfolding novel pages to one big page
            if (node.attributes.contains(QStringLiteral("class"))
                    && unhide_classes.contains(node.attributes.value(
                                                   QStringLiteral("class")).toLower().trimmed())) {
                if (!node.attributes.contains(QStringLiteral("style"))) {
                    node.attributes[QStringLiteral("style")]=QString();
                }
                if (node.attributes.value(QStringLiteral("style")).isEmpty()) {
                    node.attributes[QStringLiteral("style")]=QStringLiteral("display:block;");
                } else {
                    node.attributes[QStringLiteral("style")]=
                            node.attributes.value(QStringLiteral("style"))
                            +QStringLiteral("; display:block;");
                }
            }
        }

        idx=0;
        while(idx<node.children.count()) {
            // Style fix for 2015/05 pixiv novel viewer update - advanced workarounds
            // Remove 'vtoken' inline span
            if (node.children.at(idx).tagName.toLower()==QStringLiteral("span") &&
                    node.children.at(idx).attributes.contains(QStringLiteral("class")) &&
                    node.children.at(idx).attributes.value(QStringLiteral("class"))
                    .toLower().trimmed().startsWith(QStringLiteral("vtoken"))) {

                const QVector<CHTMLNode> subnodes = node.children.at(idx).children;
                for(int si=0;si<subnodes.count();si++)
                    node.children.insert(idx+si+1,subnodes.at(si));

                node.children.removeAt(idx);
                node.normalize();
                idx=0;
                continue;
            }

            // remove ruby annotation (superscript), unfold main text block
            if (node.children.at(idx).tagName.toLower()==QStringLiteral("ruby")) {
                subnodes.clear();
                for (const CHTMLNode& rnode : qAsConst(node.children.at(idx).children)) {
                    if (rnode.tagName.toLower()==QStringLiteral("rb"))
                        subnodes << rnode.children;
                }

                for(int si=0;si<subnodes.count();si++)
                    node.children.insert(idx+si+1,subnodes.at(si));

                node.children.removeAt(idx);
                node.normalize();
                idx=0;
                continue;
            }

            // remove floating headers for fc2 and goo blogs
            if (m_metaSrcUrl.host().contains(QRegExp(QStringLiteral("blog"),Qt::CaseInsensitive)) &&
                    node.children.at(idx).tagName.toLower()==QStringLiteral("div") &&
                    node.children.at(idx).attributes.contains(QStringLiteral("id")) &&
                    denyDivs.contains(node.children.at(idx).attributes.value(QStringLiteral("id")),
                                      Qt::CaseInsensitive)) {
                node.children.removeAt(idx);
                node.normalize();
                idx=0;
                continue;
            }

            // Remove scripts and iframes
            if (denyTags.contains(node.children.at(idx).tagName,Qt::CaseInsensitive)) {
                node.children.removeAt(idx);
                node.normalize();
                idx = 0;
                continue;
            }
            idx++;
        }

        // collecting image urls
        if (node.tagName.toLower()==QStringLiteral("img")) {
            if (node.attributes.contains(QStringLiteral("src"))) {
                QString src = node.attributes.value(QStringLiteral("src")).trimmed();
                if (!src.isEmpty())
                    m_imgUrls << src;
            }
        }
        if (node.tagName.toLower()==QStringLiteral("a")) {
            if (node.attributes.contains(QStringLiteral("href"))) {
                QString src = node.attributes.value(QStringLiteral("href")).trimmed();
                QFileInfo fi(src);
                if (acceptedExt.contains(fi.suffix(),Qt::CaseInsensitive))
                    m_imgUrls << src;
            }
        }
    }

    if (xmlPass==PXPostprocess) {
        // remove duplicated <br>
        int idx=0;
        while(idx<node.children.count()) {
            if (node.children.at(idx).tagName.toLower()==QStringLiteral("br")) {
                while(idx<(node.children.count()-1) &&
                      node.children.at(idx+1).tagName.toLower()==QStringLiteral("br")) {
                    node.children.removeAt(idx+1);
                }
            }
            idx++;
        }
    }

    if (m_translatorFailed) return;

    for(auto &child : node.children) {
        examineNode(child,xmlPass);
    }
}

bool CTranslator::translateParagraph(CHTMLNode &src, CTranslator::XMLPassMode xmlPass)
{
    if (m_tran==nullptr) return false;

    bool failure = false;

    int baseProgress = 0;
    if (m_textNodesCnt>0) {
        baseProgress = 100*m_textNodesProgress/m_textNodesCnt;
    }
    Q_EMIT setProgress(baseProgress);

    QString ssrc = src.text;
    ssrc = ssrc.replace(QStringLiteral("\r\n"),QStringLiteral("\n"));
    ssrc = ssrc.replace('\r','\n');
    ssrc = ssrc.replace(QRegExp(QStringLiteral("\n{2,}")),QStringLiteral("\n"));
    QStringList sl = ssrc.split('\n',QString::SkipEmptyParts);

    QString sout;
    sout.clear();

    QStringList srctls;
    srctls.clear();

    const QChar questionMark('?');
    const QChar fullwidthQuestionMark(0xff1f);
    const int progressUpdateFrac = 5;

    for(int i=0;i<sl.count();i++) {
        m_abortMutex.lock();
        if (m_abortFlag) {
            m_abortMutex.unlock();
            sout = tr("ATLAS: Translation thread aborted.");
            break;
        }
        m_abortMutex.unlock();

        if (xmlPass!=PXTranslate) {
            m_textNodesCnt++;
        } else {
            QString srct = sl.at(i);

            if (srct.trimmed().isEmpty()) {
                sout += QStringLiteral("\n");
            } else {
                QString t = QString();

                QString ttest = srct;
                bool noText = true;
                ttest.remove(QRegExp(QStringLiteral("&\\w+;")));
                for (const QChar &tc : qAsConst(ttest)) {
                    if (tc.isLetter()) {
                        noText = false;
                        break;
                    }
                }

                if (!noText) {
                    if (m_translateSubSentences) {
                        QString tacc = QString();
                        for (int j=0;j<srct.length();j++) {
                            QChar sc = srct.at(j);

                            if (sc.isLetter())
                                tacc += sc;
                            if (!sc.isLetter() || (j==(srct.length()-1))) {
                                if (!tacc.isEmpty()) {
                                    if (sc.isLetter()) {
                                        t += m_tran->tranString(tacc);
                                    } else {
                                        if (sc==questionMark || sc==fullwidthQuestionMark) {
                                            tacc += sc;
                                            t += m_tran->tranString(tacc);
                                        } else {
                                            t += m_tran->tranString(tacc) + sc;
                                        }
                                    }
                                    tacc.clear();
                                } else {
                                    t += sc;
                                }
                            }
                            if (!m_tran->getErrorMsg().isEmpty())
                                break;
                        }
                    } else {
                        t = m_tran->tranString(srct);
                    }
                } else {
                    t = srct;
                }

                if (m_translationMode==CStructures::tmTooltip) {
                    QString ts = srct;
                    srct = t;
                    t = ts;
                }

                if (m_translationMode==CStructures::tmAdditive)
                    sout += srct + QStringLiteral("<br/>");

                if (!m_tran->getErrorMsg().isEmpty()) {
                    m_tran->doneTran();
                    sout += t;
                    failure = true;
                    break;
                }

                if (m_useOverrideFont || m_forceFontColor) {
                    QString dstyle;
                    if (m_useOverrideFont) {
                        dstyle+=QStringLiteral("font-family: %1; font-size: %2pt;").arg(
                                    m_overrideFont.family()).arg(
                                    m_overrideFont.pointSize());
                    }
                    if (m_forceFontColor)
                        dstyle+=QStringLiteral("color: %1;").arg(m_forcedFontColor.name());

                    sout += QStringLiteral("<span style=\"%1\">%2</span>").arg(dstyle,t);
                } else {
                    sout += t;
                }

                if (m_translationMode==CStructures::tmAdditive) {
                    sout += QStringLiteral("<br/>\n");
                } else {
                    srctls << srct;
                }
            }

            if (m_textNodesCnt > 0 && (i % progressUpdateFrac == 0)) {
                int pr = 100*m_textNodesProgress/m_textNodesCnt;
                Q_EMIT setProgress(pr);
            }
            m_textNodesProgress++;
        }
    }

    if (xmlPass==PXTranslate && !sout.isEmpty()) {
        if (!srctls.isEmpty() && ((m_translationMode==CStructures::tmOverwriting)
                                  || (m_translationMode==CStructures::tmTooltip))) {
            src.text = QStringLiteral("<span title=\"%1\">%2</span>")
                       .arg(srctls.join('\n').replace('"','\''),sout);
        } else {
            src.text = sout;
        }
    }

    return !failure;
}

void CTranslator::dumpPage(QUuid token, const QString &suffix, const QString &page)
{
    QString fname = CGenericFuncs::getTmpDir() + QDir::separator() + token.toString()
            + QStringLiteral("-") + suffix + QStringLiteral(".html");
    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to create dump file " << fname;
        return;
    }
    f.write(page.toUtf8());
    f.close();
}

void CTranslator::dumpPage(QUuid token, const QString &suffix, const CHTMLNode &page)
{
    QString fname = CGenericFuncs::getTmpDir() + QDir::separator() + token.toString()
            + QStringLiteral("-") + suffix + QStringLiteral(".html");
    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to create dump file " << fname;
        return;
    }
    QString html;
    html.clear();
    generateHTML(page, html);
    f.write(html.toUtf8());
    f.close();
}

void CTranslator::dumpPage(QUuid token, const QString &suffix, const QByteArray &page)
{
    QString fname = CGenericFuncs::getTmpDir() + QDir::separator() + token.toString()
            + QStringLiteral("-") + suffix + QStringLiteral(".html");
    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to create dump file " << fname;
        return;
    }
    f.write(page);
    f.close();
}

void CTranslator::translate()
{
    QString translatedHtml;
    QString lastError;
    CLangPair lp(gSet->ui()->getActiveLangPair());
    if (!lp.isValid()) {
        lastError = tr("Translator initialization error: Unacceptable or empty translation pair.");
        Q_EMIT translationFinished(false,translatedHtml,lastError);
    }

    if (m_translationEngine==CStructures::teAtlas) {
        bool oktrans = false;

        if (!lp.isAtlasAcceptable()) {
            lastError = tr("ATLAS error: Unacceptable translation pair. Only English and Japanese is supported.");
        } else {
            for (int i=0;i<m_atlTcpRetryCount;i++) {
                if (translateDocument(m_sourceHtml,translatedHtml)) {
                    oktrans = true;
                    break;
                }
                if (m_tran) {
                    lastError = m_tran->getErrorMsg();
                } else {
                    lastError = tr("ATLAS translator failed.");
                }
                if (m_tran)
                    m_tran->doneTran(true);
                QThread::sleep(static_cast<unsigned long>(m_atlTcpTimeout));
            }
        }
        if (!oktrans) {
            Q_EMIT translationFinished(false,translatedHtml,lastError);
            Q_EMIT finished();
            return;
        }
    } else {
        if (!translateDocument(m_sourceHtml,translatedHtml)) {
            QString lastError = tr("Translator initialization error");
            if (m_tran)
                lastError = m_tran->getErrorMsg();
            Q_EMIT translationFinished(false,translatedHtml,lastError);
            Q_EMIT finished();
            return;
        }
    }

    Q_EMIT translationFinished(true,translatedHtml,QString());
    Q_EMIT finished();
}

void CTranslator::abortTranslator()
{
    // Intermediate timer, because sometimes webengine throws segfault from its insides on widget resize event
    const int abortTimerDelay = 500;

    QTimer::singleShot(abortTimerDelay,this,[this](){
        m_abortMutex.lock();
        m_abortFlag=true;
        m_abortMutex.unlock();
    });
}

void CTranslator::generateHTML(const CHTMLNode &src, QString &html, bool reformat, int depth)
{
    if (src.isTag && !src.tagName.isEmpty()) {
        html.append(QStringLiteral("<")+src.tagName);
        for (const QString &key : qAsConst(src.attributesOrder)) {
            const QString val = src.attributes.value(key);
            if (!val.contains('"')) {
                html.append(QStringLiteral(" %1=\"%2\"").arg(key,val));
            } else {
                html.append(QStringLiteral(" %1='%2'").arg(key,val));
            }
        }
        html.append(QStringLiteral(">"));
    } else {
        QString txt = src.text;
        // fix incorrect nested comments - pixiv
        if (src.isComment) {
            if ((txt.count(QStringLiteral("<!--"))>1) || (txt.count(QStringLiteral("-->"))>1))
                txt.clear();
            if ((txt.count(QStringLiteral("<"))>1 || txt.count(QStringLiteral(">"))>1)
                    && (txt.count(QStringLiteral("<"))!=txt.count(QStringLiteral(">")))) {
                txt.clear();
            }
        }
        html.append(txt);
    }

    for (const CHTMLNode &node : qAsConst(src.children))
        generateHTML(node,html,reformat,depth+1);

    html.append(src.closingText);
    if (reformat)
        html.append('\n');
}

void CTranslator::replaceLocalHrefs(CHTMLNode& node, const QUrl& baseUrl)
{
    if (node.tagName.toLower()==QStringLiteral("a")) {
        if (node.attributes.contains(QStringLiteral("href"))) {
            QUrl ref = QUrl(node.attributes.value(QStringLiteral("href")));
            if (ref.isRelative())
                node.attributes[QStringLiteral("href")]=baseUrl.resolved(ref).toString();
        }
    }

    for (auto &child : node.children) {
        replaceLocalHrefs(child,baseUrl);
    }
}

CHTMLNode::CHTMLNode()
{
    children.clear();
    attributes.clear();
    attributesOrder.clear();
    text.clear();
    tagName.clear();
    closingText.clear();
    isTag = false;
    isComment = false;
}

CHTMLNode::~CHTMLNode()
{
    children.clear();
    attributes.clear();
    attributesOrder.clear();
}

CHTMLNode::CHTMLNode(const CHTMLNode &other)
{
    text = other.text;
    tagName = other.tagName;
    closingText = other.closingText;
    isTag = other.isTag;
    isComment = other.isComment;

    attributes.clear();
    attributes = other.attributes;

    attributesOrder.clear();
    attributesOrder = other.attributesOrder;

    children.clear();
    children = other.children;
}

CHTMLNode::CHTMLNode(tree<HTML::Node> const & node)
{
    tree<HTML::Node>::iterator it = node.begin();

    it->parseAttributes();

    text = it->text();
    tagName = it->tagName();
    closingText = it->closingText();
    isTag = it->isTag();
    isComment = it->isComment();

    attributes.clear();
    attributes = it->attributes();

    attributesOrder.clear();
    attributesOrder = it->attributesOrder();

    children.clear();
    children.reserve(static_cast<int>(it.number_of_children()));
    for (unsigned i=0; i<it.number_of_children(); i++ )
        children << CHTMLNode(node.child(it,i));
}

CHTMLNode::CHTMLNode(const QString &innerText)
{
    // creates text node
    text = innerText;
    tagName = innerText;
    closingText.clear();
    isTag = false;
    isComment = false;

    attributes.clear();
    attributesOrder.clear();
    children.clear();
}

CHTMLNode &CHTMLNode::operator=(const CHTMLNode &other)
{
    text = other.text;
    tagName = other.tagName;
    closingText = other.closingText;
    isTag = other.isTag;
    isComment = other.isComment;

    attributes.clear();
    attributes = other.attributes;

    attributesOrder.clear();
    attributesOrder = other.attributesOrder;

    children.clear();
    children = other.children;

    return *this;
}

bool CHTMLNode::operator==(const CHTMLNode &s) const
{
    return (text == s.text);
}

bool CHTMLNode::operator!=(const CHTMLNode &s) const
{
    return !operator==(s);
}

void CHTMLNode::normalize()
{
    int i = 0;
    while (i<(children.count()-1)) {
        if (children.at(i).isTextNode() &&
                children.at(i+1).isTextNode()) {
            if (!children.at(i+1).text.trimmed().isEmpty())
                children[i].text += children.at(i+1).text;
            children.removeAt(i+1);
            i = 0;
            continue;
        }
        i++;
    }
}

bool CHTMLNode::isTextNode() const
{
    return (!isTag && !isComment);
}
