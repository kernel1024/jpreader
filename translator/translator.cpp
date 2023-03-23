#include <QProcess>
#include <QThread>
#include <QBuffer>
#include <QRegularExpression>
#include "translator.h"
#include "translatorcache.h"
#include "translator-workers/atlastranslator.h"
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/pythonfuncs.h"
#include <sstream>

CTranslator::CTranslator(QObject* parent, const QString& sourceHtml,
                         const QString &title, const QUrl &origin,
                         CStructures::TranslationEngine engine,
                         const CLangPair &langPair)
    : CAbstractThreadWorker(parent),
      m_retryCount(gSet->settings()->translatorRetryCount),
      m_tokensMaxCountCombined(gSet->settings()->tokensMaxCountCombined),
      m_useOverrideTransFont(gSet->actions()->useOverrideTransFont()),
      m_forceFontColor(gSet->actions()->forceFontColor()),
      m_translationEngine(engine),
      m_translationMode(gSet->actions()->getTranslationMode()),
      m_sourceHtml(sourceHtml),
      m_forcedFontColor(gSet->settings()->forcedFontColor),
      m_overrideTransFont(gSet->settings()->overrideTransFont),
      m_title(title),
      m_origin(origin),
      m_langPair(langPair)
{
    m_subsentencesMode = gSet->actions()->getSubsentencesMode(m_translationEngine);
    m_engineName = CStructures::translationEngines().value(m_translationEngine);
}

bool CTranslator::translateDocument(const QString &srcHtml, QString &dstHtml)
{
    resetAbortFlag();

    if (!m_tran && !m_tranInited) {
        m_tran.reset(CAbstractTranslator::translatorFactory(this, m_translationEngine, m_langPair));
    }

    if (!m_tran || !m_tran->initTran()) {
        dstHtml=tr("Unable to initialize translation engine.");
        qCritical() << tr("Unable to initialize translation engine.");
        return false;
    }
    m_tranInited = true;
    connect(m_tran.data(),&CAbstractTranslator::translatorBytesTransferred,
            this,&CTranslator::addTranslatorRequestBytes,Qt::DirectConnection);

    dstHtml.clear();
    if (srcHtml.isEmpty()) return false;

    const QUuid token = QUuid::createUuid();

    QString src = srcHtml.trimmed();
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("1-source"),src);
    src = src.remove(0,src.indexOf(QSL("<html"),Qt::CaseInsensitive));

    CHTMLNode doc = CHTMLParser::parseHTML(src);

    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("2-converted"),doc);

    m_translatorFailed = false;
    m_textNodesCnt=0;
    m_metaSrcUrl.clear();
    examineNode(doc,PXPreprocess);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("3-preprocessed"),doc);

    examineNode(doc,PXCalculate);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("4-calculated"),doc);

    m_textNodesProgress=0;
    examineNode(doc,PXTranslate);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("5-translated"),doc);

    examineNode(doc,PXPostprocess);
    CHTMLParser::generateHTML(doc,dstHtml);

    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("6-finalized"),dstHtml);

    m_tran->doneTran();

    return !m_translatorFailed;
}

bool CTranslator::documentReparse(const QString &sourceHtml, QString &destHtml)
{
    resetAbortFlag();

    destHtml.clear();
    if (sourceHtml.isEmpty()) return false;

    const QUuid token = QUuid::createUuid();

    QString src = sourceHtml.trimmed();
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("parser-1-source"),src);
    src = src.remove(0,src.indexOf(QSL("<html"),Qt::CaseInsensitive));

    CHTMLNode doc = CHTMLParser::parseHTML(src);

    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("parser-2-converted"),doc);

    m_translatorFailed=false;
    m_textNodesCnt=0;
    m_metaSrcUrl.clear();
    examineNode(doc,PXPreprocess);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("parser-3-preprocessed"),doc);

    examineNode(doc,PXCalculate);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("parser-4-calculated"),doc);

    m_textNodesProgress=0;
    examineNode(doc,PXTranslate);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("parser-5-translated"),doc);

    CHTMLParser::generateHTML(doc,destHtml);
    if (gSet->settings()->debugDumpHtml)
        dumpPage(token,QSL("parser-6-finalized"),destHtml);

    return true;
}

QStringList CTranslator::getImgUrls() const
{
    return m_imgUrls;
}

QStringList CTranslator::getAnchorUrls() const
{
    return m_allAnchorUrls;
}

QString CTranslator::workerDescription() const
{
    if (m_tran.isNull())
        return tr("Translator (initializing)");

    return tr("Translator %1 (%2)")
            .arg(CStructures::translationEngines().value(m_tran->engine()),
                 m_tran->language().toShortString());
}

void CTranslator::examineNode(CHTMLNode &node, CTranslator::XMLPassMode xmlPass)
{
    if (m_translatorFailed) return;

    if (isAborted()) return;

    QApplication::processEvents();

    static const QStringList skipTags { QSL("style"), QSL("title") };
    static const QStringList denyTags { QSL("script"), QSL("noscript"),
                QSL("object"), QSL("iframe") };
    static const QStringList denyDivs { QSL("sh_fc2blogheadbar"),
                QSL("fc2_bottom_bnr"), QSL("global-header") };

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
        if (node.tagName.toLower()==QSL("meta")) {
            if (node.attributes.value(QSL("http-equiv")).toLower().trimmed()==QSL("content-type"))
                node.attributes[QSL("content")] = QSL("text/html; charset=UTF-8");
        }

        // Unfold "1% height" divs from blog.jp/livedoor.jp
        // their blogs using same site.css
        // Also for blog.goo.ne.jp (same CSS as static.css)
        static const QRegularExpression blogJpSiteCss(QSL("blog.*\\.jp.*site.css\\?_="),
                                                      QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression gooJpStaticCss(QSL("/css/.*static.css\\?"),
                                                       QRegularExpression::CaseInsensitiveOption);
        int idx = 0;
        while (idx<node.children.count()) {
            if (node.children.at(idx).tagName.toLower()==QSL("meta")) {
                if (node.children.at(idx).attributes.value(QSL("property"))
                        .toLower().trimmed()==QSL("og:url"))
                    m_metaSrcUrl = QUrl(node.children.at(idx).attributes.value(QSL("content")).toLower().trimmed());
            }

            if (node.children.at(idx).tagName.toLower()==QSL("link")) {
                if (node.children.at(idx).attributes.value(QSL("rel"))
                        .toLower().trimmed()==QSL("canonical"))
                    m_metaSrcUrl = QUrl(node.children.at(idx).attributes.value(QSL("href")).toLower().trimmed());

                if ((node.children.at(idx).attributes.value(QSL("type"))
                     .toLower().trimmed()==QSL("text/css")) &&
                        (node.children.at(idx).attributes.value(QSL("href")).contains(blogJpSiteCss) ||
                         (node.children.at(idx).attributes.value(QSL("href")).contains(gooJpStaticCss) &&
                          m_metaSrcUrl.host().contains(QSL("blog"),Qt::CaseInsensitive)))) {
                    CHTMLNode st;
                    st.tagName=QSL("style");
                    st.closingText=QSL("</style>");
                    st.isTag=true;
                    st.children << CHTMLNode(QSL("* { height: auto !important; }"));
                    node.children.insert(idx+1,st);
                    break;
                }
            }
            idx++;
        }

        // div processing
        if (node.tagName.toLower()==QSL("div")) {

            // unfold compressed divs
            QString sdivst;
            if (node.attributes.contains(QSL("style")))
                sdivst = node.attributes.value(QSL("style"));
            if (!sdivst.contains(QSL("absolute"),Qt::CaseInsensitive)) {
                if (!node.attributes.contains(QSL("style"))) {
                    node.attributes[QSL("style")]=QString();
                }
                if (node.attributes.value(QSL("style")).isEmpty()) {
                    node.attributes[QSL("style")]=QSL("height:auto;");
                } else {
                    node.attributes[QSL("style")]=
                            node.attributes.value(QSL("style"))
                            +QSL("; height:auto;");
                }
            }
        }

        // replace <br>'s with text tags, then combine sequence of text tags into one big entity
        idx=0;
        while(idx<node.children.count()) {
            if (node.children.at(idx).tagName.toLower()==QSL("br")) {
                const CHTMLNode st(QSL("\n")); // NOTE: or something other as a marker?
                node.children.insert(idx+1,st);
                node.children.removeAt(idx);
            }
            idx++;
        }
        node.normalize(); // force combine with other text tags

        idx=0;
        while(idx<node.children.count()) {
            // remove ruby annotation (superscript), unfold main text block
            if (node.children.at(idx).tagName.toLower()==QSL("ruby")) {
                subnodes.clear();
                for (const CHTMLNode& rnode : qAsConst(node.children.at(idx).children)) {
                    if (rnode.tagName.toLower()==QSL("rb"))
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
            if (m_metaSrcUrl.host().contains(QSL("blog"),Qt::CaseInsensitive) &&
                    node.children.at(idx).tagName.toLower()==QSL("div") &&
                    node.children.at(idx).attributes.contains(QSL("id")) &&
                    denyDivs.contains(node.children.at(idx).attributes.value(QSL("id")),
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
        if (node.tagName.toLower()==QSL("img")) {
            if (node.attributes.contains(QSL("src"))) {
                const QString src = node.attributes.value(QSL("src")).trimmed();
                if (!src.isEmpty())
                    m_imgUrls.append(src);
            }
        }
        if (node.tagName.toLower()==QSL("a")) {
            if (node.attributes.contains(QSL("href"))) {
                const QString src = node.attributes.value(QSL("href")).trimmed();
                m_allAnchorUrls.append(src);
                const QFileInfo fi(src);
                if (acceptedExt.contains(fi.suffix(),Qt::CaseInsensitive))
                    m_imgUrls.append(src);
            }
        }
    }

    if (xmlPass==PXPostprocess) {
        // remove duplicated <br>
        int idx=0;
        while(idx<node.children.count()) {
            if (node.children.at(idx).tagName.toLower()==QSL("br")) {
                while(idx<(node.children.count()-1) &&
                      node.children.at(idx+1).tagName.toLower()==QSL("br")) {
                    node.children.removeAt(idx+1);
                }
            }
            idx++;
        }

        if (node.tagName.toLower()==QSL("span") &&
                node.attributes.contains(QSL("id")) &&
                node.attributes.value(QSL("id"))==QSL("jpreader_translator_desc")) {
            node.children.clear();
            node.children.append(CHTMLNode(tr("Translated with %1.").arg(m_engineName)));
            node.normalize();
        }
    }

    if (m_translatorFailed) return;

    for(auto &child : node.children) {
        examineNode(child,xmlPass);
    }
}

bool CTranslator::translateParagraph(CHTMLNode &src, CTranslator::XMLPassMode xmlPass)
{
    static const QRegularExpression htmlEntities(QSL("&\\w+;"));
    static const QRegularExpression multiNewline(QSL("\n{2,}"));
    static const QChar questionMark(u'?');
    static const QChar fullwidthQuestionMark(u'\uFF1F');

    if (!m_tran) return false;

    bool failure = false;

    int baseProgress = 0;
    if (m_textNodesCnt>0) {
        baseProgress = 100*m_textNodesProgress/m_textNodesCnt;
    }
    Q_EMIT setProgress(baseProgress);

    QString srcTemp = src.text;
    srcTemp = srcTemp.replace(QSL("\r\n"),QSL("\n"));
    srcTemp = srcTemp.replace(u'\r',u'\n');
    srcTemp = srcTemp.replace(multiNewline,QSL("\n"));
    const QStringList sourceStrings = srcTemp.split(u'\n',Qt::SkipEmptyParts);

    QString translatedOutput;
    QStringList translatedOutputList;
    QStringList combineAccumulator;
    int combinedTokenCount = 0;

    const int progressUpdateFrac = 5;

    for (int idx = 0; idx < sourceStrings.count(); idx++) {
        if (isAborted()) {
            translatedOutput = tr("ATLAS: Translation thread aborted.");
            break;
        }

        if (xmlPass!=PXTranslate) {
            m_textNodesCnt++;
        } else {
            QString sourceStrTemp = sourceStrings.at(idx);

            if (sourceStrTemp.trimmed().isEmpty()) {
                translatedOutput += u'\n';
            } else {

                QString ttest = sourceStrTemp;
                bool noText = true;
                ttest.remove(htmlEntities);
                for (const QChar &tc : qAsConst(ttest)) {
                    if (tc.isLetter()) {
                        noText = false;
                        break;
                    }
                }

                QString tranResult;
                if (!noText) {
                    switch (m_subsentencesMode) {
                        case CStructures::smCombineToMaxTokens: {// Preferred mode for AI translators
                            combineAccumulator.append(sourceStrTemp);
                            combinedTokenCount += gSet->python()->tiktokenCountTokens(sourceStrTemp);
                            if ((combinedTokenCount >= m_tokensMaxCountCombined) ||  // max tokens
                                    ((idx + 1) >= sourceStrings.count())) {          // or last string in the list
                                sourceStrTemp = combineAccumulator.join(QChar('\n'));
                                tranResult = m_tran->tranString(sourceStrTemp);
                                combineAccumulator.clear();
                                combinedTokenCount = 0;
                            }
                            break;
                        }
                        case CStructures::smSplitByPunctuation: { // This is preferred mode for ATLAS
                            QString sourceStrPart;
                            for (int j=0;j<sourceStrTemp.length();j++) {
                                const QChar schar = sourceStrTemp.at(j);

                                if (schar.isLetterOrNumber())
                                    sourceStrPart += schar;
                                if (!schar.isLetterOrNumber() || (j==(sourceStrTemp.length()-1))) {
                                    if (!sourceStrPart.isEmpty()) {
                                        if (schar.isLetterOrNumber()) {
                                            tranResult += m_tran->tranString(sourceStrPart);
                                        } else {
                                            if (schar==questionMark || schar==fullwidthQuestionMark) {
                                                sourceStrPart += schar;
                                                tranResult += m_tran->tranString(sourceStrPart);
                                            } else {
                                                tranResult += m_tran->tranString(sourceStrPart) + schar;
                                            }
                                        }
                                        sourceStrPart.clear();
                                    } else {
                                        tranResult += schar;
                                    }
                                }
                                if (!m_tran->getErrorMsg().isEmpty())
                                    break;
                            }
                            break;
                        }
                        case CStructures::smKeepParagraph: // Preferred mode for non-AI translators
                            tranResult = m_tran->tranString(sourceStrTemp);
                            break;
                    }
                } else {
                    tranResult = sourceStrTemp;
                }

                if (!m_tran->getErrorMsg().isEmpty()) {
                    m_tran->doneTran();
                    translatedOutput += tranResult;
                    failure = true;
                    break;
                }

                if (!noText && tranResult.isEmpty()) {
                    // Skip, we are combining now
                } else {
                    if (m_translationMode==CStructures::tmTooltip) {
                        const QString swapper = sourceStrTemp;
                        sourceStrTemp = tranResult;
                        tranResult = swapper;
                    }

                    if (m_translationMode==CStructures::tmAdditive)
                        translatedOutput += sourceStrTemp + QSL("<br/>");

                    if (m_useOverrideTransFont || m_forceFontColor) {
                        QString dstyle;
                        if (m_useOverrideTransFont) {
                            dstyle+=QSL("font-family: %1; font-size: %2pt;").arg(
                                        m_overrideTransFont.family()).arg(
                                        m_overrideTransFont.pointSize());
                        }
                        if (m_forceFontColor)
                            dstyle+=QSL("color: %1;").arg(m_forcedFontColor.name());

                        translatedOutput += QSL("<span style=\"%1\">%2</span>").arg(dstyle,tranResult);
                    } else {
                        translatedOutput += tranResult;
                    }

                    if (m_translationMode==CStructures::tmAdditive) {
                        translatedOutput += QSL("\n");
                    } else {
                        translatedOutputList.append(sourceStrTemp);
                    }
                }
            }

            if (m_textNodesCnt > 0 && (idx % progressUpdateFrac == 0)) {
                const int progress = 100*m_textNodesProgress/m_textNodesCnt;
                Q_EMIT setProgress(progress);
            }
            m_textNodesProgress++;
        }
    }

    if (xmlPass==PXTranslate && !translatedOutput.isEmpty()) {

        translatedOutput = translatedOutput.replace(QSL("\n"),QSL("<br/>"));

        if (!translatedOutputList.isEmpty() && ((m_translationMode==CStructures::tmOverwriting)
                                  || (m_translationMode==CStructures::tmTooltip))) {
            src.text = QSL("<span title=\"%1\">%2</span>")
                       .arg(translatedOutputList.join(u'\n').replace(u'"',u'\''),translatedOutput);
        } else {
            src.text = translatedOutput;
        }
    }

    return !failure;
}

void CTranslator::dumpPage(QUuid token, const QString &suffix, const QString &page)
{
    const QString fname = CGenericFuncs::getTmpDir() + QDir::separator() + token.toString()
                          + QSL("-") + suffix + QSL(".html");
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
    const QString fname = CGenericFuncs::getTmpDir() + QDir::separator() + token.toString()
                          + QSL("-") + suffix + QSL(".html");
    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to create dump file " << fname;
        return;
    }
    QString html;
    CHTMLParser::generateHTML(page, html);
    f.write(html.toUtf8());
    f.close();
}

void CTranslator::dumpPage(QUuid token, const QString &suffix, const QByteArray &page)
{
    const QString fname = CGenericFuncs::getTmpDir() + QDir::separator() + token.toString()
                          + QSL("-") + suffix + QSL(".html");
    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to create dump file " << fname;
        return;
    }
    f.write(page);
    f.close();
}

void CTranslator::startMain()
{
    QString translatedHtml;
    QString lastError;
    if (!m_langPair.isValid()) {
        lastError = tr("Translator initialization error: Unacceptable or empty translation pair.");
        Q_EMIT translationFinished(false,isAborted(),translatedHtml,lastError);
        Q_EMIT finished();
        return;
    }

    if (gSet->settings()->translatorCacheEnabled) {
        translatedHtml = gSet->translatorCache()->cachedTranslatorResult(
                             m_sourceHtml,
                             m_langPair,
                             m_translationEngine,
                             m_subsentencesMode);
        if (!translatedHtml.isEmpty()) {
            Q_EMIT translationFinished(true,isAborted(),translatedHtml,QString());
            Q_EMIT finished();
            return;
        }
    }

    if (m_translationEngine==CStructures::teAtlas) {
        bool oktrans = false;

        if (!m_langPair.isAtlasAcceptable()) {
            lastError = tr("ATLAS error: Unacceptable translation pair. Only English and Japanese is supported.");
        } else {
            for (int i=0;i<m_retryCount;i++) {
                if (translateDocument(m_sourceHtml,translatedHtml)) {
                    oktrans = true;
                    break;
                }
                if (m_tran) {
                    lastError = m_tran->getErrorMsg();
                } else {
                    lastError = tr("ATLAS translator failed.");
                }
                if (m_tran) {
                    m_tran->doneTran(true);
                    QThread::msleep(m_tran->getRandomDelay(CDefaults::atlasMinRetryDelay,
                                                           CDefaults::atlasMaxRetryDelay));
                }
            }
        }
        if (!oktrans) {
            Q_EMIT translationFinished(false,isAborted(),translatedHtml,lastError);
            Q_EMIT finished();
            return;
        }
    } else {
        if (!translateDocument(m_sourceHtml,translatedHtml)) {
            QString lastError = tr("Translator initialization error");
            if (m_tran)
                lastError = m_tran->getErrorMsg();
            Q_EMIT translationFinished(false,isAborted(),translatedHtml,lastError);
            Q_EMIT finished();
            return;
        }
    }

    if (gSet->settings()->translatorCacheEnabled &&
            !isAborted()) {
        gSet->translatorCache()->saveTranslatorResult(m_sourceHtml,translatedHtml,m_langPair,
                                                      m_translationEngine,m_subsentencesMode,
                                                      m_title,m_origin);
    }

    Q_EMIT translationFinished(true,isAborted(),translatedHtml,QString());
    Q_EMIT finished();
}

void CTranslator::addTranslatorRequestBytes(qint64 size)
{
    addLoadedRequest(size);
}
