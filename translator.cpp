#include <QProcess>
#include <QBuffer>
#include "translator.h"
#include "snviewer.h"
#include "genericfuncs.h"
#include <sstream>

using namespace htmlcxx;

CTranslator::CTranslator(QObject* parent, const QString& aUri, bool forceTranSubSentences)
    : QObject(parent)
{
    hostingDir=gSet->settings.hostingDir;
    hostingUrl=gSet->settings.hostingUrl;
    createdFiles=&(gSet->createdFiles);
    Uri=aUri;
    useSCP=gSet->settings.useScp;
    scpParams=gSet->settings.scpParams;
    scpHost=gSet->settings.scpHost;
    translationEngine=gSet->settings.translatorEngine;
    if (!hostingDir.endsWith('/')) hostingDir.append('/');
    if (!hostingUrl.endsWith('/')) hostingUrl.append('/');
    atlTcpRetryCount=gSet->settings.atlTcpRetryCount;
    atlTcpTimeout=gSet->settings.atlTcpTimeout;
    forceFontColor=gSet->ui.forceFontColor();
    forcedFontColor=gSet->settings.forcedFontColor;
    useOverrideFont=gSet->ui.useOverrideFont();
    overrideFont=gSet->settings.overrideFont;
    translationMode=gSet->ui.getTranslationMode();
    engine=gSet->settings.translatorEngine;
    translateSubSentences=(forceTranSubSentences || gSet->ui.translateSubSentences());
    tran=nullptr;
    tranInited=false;
    metaSrcUrl.clear();
    imgUrls.clear();
    textNodesCnt = 0;
    textNodesProgress = 0;
    localHosting = false;
    abortFlag = false;
    translatorFailed = false;
}

CTranslator::~CTranslator()
{
    if (tran)
        tran->deleteLater();
}

bool CTranslator::calcLocalUrl(const QString& aUri, QString& calculatedUrl)
{
    QString wdir = hostingDir;
    if (wdir.isEmpty()
            || hostingUrl.isEmpty()
            || !hostingUrl.startsWith(QStringLiteral("http"),Qt::CaseInsensitive)) return false;
    QString filename = QUrl(aUri).toLocalFile();
    if (filename.isEmpty()) {
        calculatedUrl=aUri;
        return true;
    }
    if (!useSCP) {
        QFileInfo fd(wdir);
        if (!fd.exists()) {
            QDir qd;
            qd.mkpath(wdir);
        }
    }
    QFileInfo fi(filename);
    QString wname=gSet->makeTmpFileName(fi.completeSuffix());

    calculatedUrl.clear();

    bool b,r;
    if (useSCP) {
        QStringList scpp;
        scpp << scpParams.split(' ');
        scpp << filename;
        scpp << QStringLiteral("%1:%2%3").arg(scpHost,wdir,wname);
        r=(QProcess::execute(QStringLiteral("scp"),scpp)==0);
        if (r) calculatedUrl=hostingUrl+wname;
        return r;
    }

    r=QFile(filename).copy(wdir+wname);
    if (r) {
        if (createdFiles) createdFiles->append(wdir+wname);
        calculatedUrl=hostingUrl+wname;
        b = true;
    } else
        b = false;
    return b;
}

bool CTranslator::translateDocument(const QString &srcUri, QString &dst)
{
    abortMutex.lock();
    abortFlag=false;
    abortMutex.unlock();

    if (tran==nullptr && !tranInited) {
        tran = translatorFactory(this, CLangPair(gSet->ui.getActiveLangPair()));
    }

    if (tran==nullptr || !tran->initTran()) {
        dst=tr("Unable to initialize translation engine.");
        qCritical() << tr("Unable to initialize translation engine.");
        return false;
    }
    tranInited = true;

    dst.clear();
    if (srcUri.isEmpty()) return false;

    QUuid token = QUuid::createUuid();

    QString src = srcUri.trimmed();
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("1-source"),src);
    src = src.remove(0,src.indexOf(QStringLiteral("<html"),Qt::CaseInsensitive));

    HTML::ParserDom parser;
    parser.parse(src);

    tree<HTML::Node> tr = parser.getTree();

    if (gSet->settings.debugDumpHtml) {
        std::stringstream sst;
        sst << tr;
        dumpPage(token,QStringLiteral("2-tree"),QByteArray::fromRawData(sst.str().data(),
                                                        static_cast<int>(sst.str().size())));
    }

    CHTMLNode doc(tr);

    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("3-converted"),doc);

    translatorFailed = false;
    textNodesCnt=0;
    metaSrcUrl.clear();
    examineNode(doc,PXPreprocess);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("4-preprocessed"),doc);

    examineNode(doc,PXCalculate);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("5-calculated"),doc);

    textNodesProgress=0;
    examineNode(doc,PXTranslate);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("6-translated"),doc);

    examineNode(doc,PXPostprocess);
    generateHTML(doc,dst);

    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("7-finalized"),dst);

    tran->doneTran();

    return !translatorFailed;
}

bool CTranslator::documentReparse(const QString &srcUri, QString &dst)
{
    abortMutex.lock();
    abortFlag=false;
    abortMutex.unlock();

    dst.clear();
    if (srcUri.isEmpty()) return false;

    QUuid token = QUuid::createUuid();

    QString src = srcUri.trimmed();
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-1-source"),src);
    src = src.remove(0,src.indexOf(QStringLiteral("<html"),Qt::CaseInsensitive));

    HTML::ParserDom parser;
    parser.parse(src);

    tree<HTML::Node> tr = parser.getTree();

    if (gSet->settings.debugDumpHtml) {
        std::stringstream sst;
        sst << tr;
        dumpPage(token,QStringLiteral("parser-2-tree"),QByteArray::fromRawData(sst.str().data(),
                                                               static_cast<int>(sst.str().size())));
    }

    CHTMLNode doc(tr);

    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-3-converted"),doc);

    translatorFailed=false;
    textNodesCnt=0;
    metaSrcUrl.clear();
    examineNode(doc,PXPreprocess);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-4-preprocessed"),doc);

    examineNode(doc,PXCalculate);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-5-calculated"),doc);

    textNodesProgress=0;
    examineNode(doc,PXTranslate);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-6-translated"),doc);

    generateHTML(doc,dst);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,QStringLiteral("parser-7-finalized"),dst);

    return true;
}

QStringList CTranslator::getImgUrls() const
{
    return imgUrls;
}

void CTranslator::examineNode(CHTMLNode &node, CTranslator::XMLPassMode xmlPass)
{
    if (translatorFailed) return;
    abortMutex.lock();
    if (abortFlag) {
        abortMutex.unlock();
        return;
    }
    abortMutex.unlock();

    QApplication::processEvents();

    static const QStringList skipTags { "style", "title" };
    static const QStringList unhide_classes { "novel-text-wrapper", "novel-pages", "novel-page" };
    static const QStringList denyTags { "script", "noscript", "object", "iframe" };
    static const QStringList denyDivs { "sh_fc2blogheadbar", "fc2_bottom_bnr", "global-header" };
    const QStringList acceptedExt = getSupportedImageExtensions();

    QVector<CHTMLNode> subnodes;

    if ((xmlPass==PXTranslate || xmlPass==PXCalculate) && tranInited) {
        if (skipTags.contains(node.m_tagName,Qt::CaseInsensitive)) return;

        if (xmlPass==PXCalculate && !node.m_isTag && !node.m_isComment) {
            translateParagraph(node,xmlPass);
        }
        if (xmlPass==PXTranslate && !node.m_isTag && !node.m_isComment) {
            if (!translateParagraph(node,xmlPass)) translatorFailed=true;
        }
    }

    if (xmlPass==PXPreprocess) {
        // WebEngine gives UTF-8 encoded page
        if (node.m_tagName.toLower()==QStringLiteral("meta")) {
            if (node.m_attributes.value(QStringLiteral("http-equiv")).toLower().trimmed()==QStringLiteral("content-type"))
                node.m_attributes[QStringLiteral("content")] = QStringLiteral("text/html; charset=UTF-8");
        }

        // Unfold "1% height" divs from blog.jp/livedoor.jp
        // their blogs using same site.css
        // Also for blog.goo.ne.jp (same CSS as static.css)
        int idx = 0;
        while (idx<node.m_children.count()) {
            if (node.m_children.at(idx).m_tagName.toLower()==QStringLiteral("meta")) {
                if (node.m_children.at(idx).m_attributes.value(QStringLiteral("property"))
                        .toLower().trimmed()==QStringLiteral("og:url"))
                    metaSrcUrl = QUrl(node.m_children.at(idx).m_attributes.value(QStringLiteral("content")).toLower().trimmed());
            }

            if (node.m_children.at(idx).m_tagName.toLower()==QStringLiteral("link")) {
                if (node.m_children.at(idx).m_attributes.value(QStringLiteral("rel"))
                        .toLower().trimmed()==QStringLiteral("canonical"))
                    metaSrcUrl = QUrl(node.m_children.at(idx).m_attributes.value(QStringLiteral("href")).toLower().trimmed());

                if ((node.m_children.at(idx).m_attributes.value(QStringLiteral("type"))
                     .toLower().trimmed()==QStringLiteral("text/css")) &&
                        (node.m_children.at(idx).m_attributes.value(QStringLiteral("href")).contains(
                             QRegExp(QStringLiteral("blog.*\\.jp.*site.css\\?_="),Qt::CaseInsensitive)) ||
                         (node.m_children.at(idx).m_attributes.value(QStringLiteral("href")).contains(
                             QRegExp(QStringLiteral("/css/.*static.css\\?"),Qt::CaseInsensitive)) &&
                          metaSrcUrl.host().contains(QRegExp(QStringLiteral("blog"),Qt::CaseInsensitive))))) {
                    CHTMLNode st;
                    st.m_tagName=QStringLiteral("style");
                    st.m_closingText=QStringLiteral("</style>");
                    st.m_isTag=true;
                    st.m_children << CHTMLNode(QStringLiteral("* { height: auto !important; }"));
                    node.m_children.insert(idx+1,st);
                    break;
                }
            }
            idx++;
        }

        // div processing
        if (node.m_tagName.toLower()==QStringLiteral("div")) {

            // unfold compressed divs
            QString sdivst;
            if (node.m_attributes.contains(QStringLiteral("style")))
                sdivst = node.m_attributes.value(QStringLiteral("style"));
            if (!sdivst.contains(QStringLiteral("absolute"),Qt::CaseInsensitive)) {
                if (!node.m_attributes.contains(QStringLiteral("style")))
                    node.m_attributes[QStringLiteral("style")]=QString();
                if (node.m_attributes.value(QStringLiteral("style")).isEmpty())
                    node.m_attributes[QStringLiteral("style")]=QStringLiteral("height:auto;");
                else
                    node.m_attributes[QStringLiteral("style")]=node.m_attributes.value(QStringLiteral("style"))+QStringLiteral("; height:auto;");
            }

            // pixiv - unfolding novel pages to one big page
            if (node.m_attributes.contains(QStringLiteral("class")) &&
                    unhide_classes.contains(node.m_attributes.value(QStringLiteral("class")).toLower().trimmed())) {
                if (!node.m_attributes.contains(QStringLiteral("style")))
                    node.m_attributes[QStringLiteral("style")]=QString();
                if (node.m_attributes.value(QStringLiteral("style")).isEmpty())
                    node.m_attributes[QStringLiteral("style")]=QStringLiteral("display:block;");
                else
                    node.m_attributes[QStringLiteral("style")]=node.m_attributes.value(QStringLiteral("style"))+QStringLiteral("; display:block;");
            }
        }

        idx=0;
        while(idx<node.m_children.count()) {
            // Style fix for 2015/05 pixiv novel viewer update - advanced workarounds
            // Remove 'vtoken' inline span
            if (node.m_children.at(idx).m_tagName.toLower()==QStringLiteral("span") &&
                    node.m_children.at(idx).m_attributes.contains(QStringLiteral("class")) &&
                    node.m_children.at(idx).m_attributes.value(QStringLiteral("class"))
                    .toLower().trimmed().startsWith(QStringLiteral("vtoken"))) {

                const QVector<CHTMLNode> subnodes = node.m_children.at(idx).m_children;
                for(int si=0;si<subnodes.count();si++)
                    node.m_children.insert(idx+si+1,subnodes.at(si));

                node.m_children.removeAt(idx);
                node.normalize();
                idx=0;
                continue;
            }

            // remove ruby annotation (superscript), unfold main text block
            if (node.m_children.at(idx).m_tagName.toLower()==QStringLiteral("ruby")) {
                subnodes.clear();
                for (const CHTMLNode& rnode : qAsConst(node.m_children.at(idx).m_children))
                    if (rnode.m_tagName.toLower()==QStringLiteral("rb"))
                        subnodes << rnode.m_children;

                for(int si=0;si<subnodes.count();si++)
                    node.m_children.insert(idx+si+1,subnodes.at(si));

                node.m_children.removeAt(idx);
                node.normalize();
                idx=0;
                continue;
            }

            // remove floating headers for fc2 and goo blogs
            if (metaSrcUrl.host().contains(QRegExp(QStringLiteral("blog"),Qt::CaseInsensitive)) &&
                    node.m_children.at(idx).m_tagName.toLower()==QStringLiteral("div") &&
                    node.m_children.at(idx).m_attributes.contains(QStringLiteral("id")) &&
                    denyDivs.contains(node.m_children.at(idx).m_attributes.value(QStringLiteral("id")),
                                      Qt::CaseInsensitive)) {
                node.m_children.removeAt(idx);
                node.normalize();
                idx=0;
                continue;
            }

            // Remove scripts and iframes
            if (denyTags.contains(node.m_children.at(idx).m_tagName,Qt::CaseInsensitive)) {
                node.m_children.removeAt(idx);
                node.normalize();
                idx = 0;
                continue;
            }
            idx++;
        }

        // collecting image urls
        if (node.m_tagName.toLower()==QStringLiteral("img")) {
            if (node.m_attributes.contains(QStringLiteral("src"))) {
                QString src = node.m_attributes.value(QStringLiteral("src")).trimmed();
                if (!src.isEmpty())
                    imgUrls << src;
            }
        }
        if (node.m_tagName.toLower()==QStringLiteral("a")) {
            if (node.m_attributes.contains(QStringLiteral("href"))) {
                QString src = node.m_attributes.value(QStringLiteral("href")).trimmed();
                QFileInfo fi(src);
                if (acceptedExt.contains(fi.suffix(),Qt::CaseInsensitive))
                    imgUrls << src;
            }
        }
    }

    if (xmlPass==PXPostprocess) {
        // remove duplicated <br>
        int idx=0;
        while(idx<node.m_children.count()) {
            if (node.m_children.at(idx).m_tagName.toLower()==QStringLiteral("br")) {
                while(idx<(node.m_children.count()-1) &&
                      node.m_children.at(idx+1).m_tagName.toLower()==QStringLiteral("br")) {
                    node.m_children.removeAt(idx+1);
                }
            }
            idx++;
        }
    }

    if (translatorFailed) return;

    for(auto &child : node.m_children) {
        examineNode(child,xmlPass);
    }
}

bool CTranslator::translateParagraph(CHTMLNode &src, CTranslator::XMLPassMode xmlPass)
{
    if (tran==nullptr) return false;

    bool failure = false;

    int baseProgress = 0;
    if (textNodesCnt>0) {
        baseProgress = 100*textNodesProgress/textNodesCnt;
    }
    emit setProgress(baseProgress);

    QString ssrc = src.m_text;
    ssrc = ssrc.replace(QStringLiteral("\r\n"),QStringLiteral("\n"));
    ssrc = ssrc.replace('\r','\n');
    ssrc = ssrc.replace(QRegExp(QStringLiteral("\n{2,}")),QStringLiteral("\n"));
    QStringList sl = ssrc.split('\n',QString::SkipEmptyParts);

    QString sout;
    sout.clear();

    QStringList srctls;
    srctls.clear();

    for(int i=0;i<sl.count();i++) {
        abortMutex.lock();
        if (abortFlag) {
            abortMutex.unlock();
            sout = tr("ATLAS: Translation thread aborted.");
            break;
        }
        abortMutex.unlock();

        if (xmlPass!=PXTranslate) {
            textNodesCnt++;
        } else {
            QString srct = sl.at(i);

            if (srct.trimmed().isEmpty())
                sout += QStringLiteral("\n");
            else {
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
                    if (translateSubSentences) {
                        QString tacc = QString();
                        for (int j=0;j<srct.length();j++) {
                            QChar sc = srct.at(j);

                            if (sc.isLetter())
                                tacc += sc;
                            if (!sc.isLetter() || (j==(srct.length()-1))) {
                                if (!tacc.isEmpty()) {
                                    if (sc.isLetter())
                                        t += tran->tranString(tacc);
                                    else {
                                        if (sc=='?' || sc==QChar(0xff1f)) {
                                            tacc += sc;
                                            t += tran->tranString(tacc);
                                        } else
                                            t += tran->tranString(tacc) + sc;
                                    }
                                    tacc.clear();
                                } else
                                    t += sc;
                            }
                            if (!tran->getErrorMsg().isEmpty())
                                break;
                        }
                    } else
                        t = tran->tranString(srct);
                } else
                    t = srct;

                if (translationMode==TM_TOOLTIP) {
                    QString ts = srct;
                    srct = t;
                    t = ts;
                }

                if (translationMode==TM_ADDITIVE)
                    sout += srct + QStringLiteral("<br/>");

                if (!tran->getErrorMsg().isEmpty()) {
                    tran->doneTran();
                    sout += t;
                    failure = true;
                    break;
                }

                if (useOverrideFont || forceFontColor) {
                    QString dstyle;
                    if (useOverrideFont)
                        dstyle+=QStringLiteral("font-family: %1; font-size: %2pt;").arg(
                                    overrideFont.family()).arg(
                                    overrideFont.pointSize());
                    if (forceFontColor)
                        dstyle+=QStringLiteral("color: %1;").arg(forcedFontColor.name());

                    sout += QStringLiteral("<span style=\"%1\">%2</span>").arg(dstyle,t);
                } else
                    sout += t;

                if (translationMode==TM_ADDITIVE)
                    sout += QStringLiteral("<br/>\n");
                else
                    srctls << srct;

            }

            if (textNodesCnt>0 && i%5==0) {
                int pr = 100*textNodesProgress/textNodesCnt;
                emit setProgress(pr);
            }
            textNodesProgress++;
        }
    }

    if (xmlPass==PXTranslate && !sout.isEmpty()) {
        if (!srctls.isEmpty() && ((translationMode==TM_OVERWRITING) || (translationMode==TM_TOOLTIP)))
            src.m_text = QStringLiteral("<span title=\"%1\">%2</span>")
                       .arg(srctls.join('\n').replace('"','\''),sout);
        else
            src.m_text = sout;
    }

    return !failure;
}

void CTranslator::dumpPage(QUuid token, const QString &suffix, const QString &page)
{
    QString fname = getTmpDir() + QDir::separator() + token.toString()
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
    QString fname = getTmpDir() + QDir::separator() + token.toString()
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
    QString fname = getTmpDir() + QDir::separator() + token.toString()
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
    QString aUrl;
    QString lastError;
    CLangPair lp = gSet->ui.getActiveLangPair();
    if (!lp.isValid()) {
        lastError = tr("Translator initialization error: Unacceptable or empty translation pair.");
        emit calcFinished(false,aUrl,lastError);
    }

    if (translationEngine==TE_ATLAS) {
        bool oktrans = false;

        if (!lp.isAtlasAcceptable()) {
            lastError = tr("ATLAS error: Unacceptable translation pair. Only English and Japanese is supported.");
        } else {
            for (int i=0;i<atlTcpRetryCount;i++) {
                if (translateDocument(Uri,aUrl)) {
                    oktrans = true;
                    break;
                }
                if (tran)
                    lastError = tran->getErrorMsg();
                else
                    lastError = tr("ATLAS translator failed.");
                if (tran)
                    tran->doneTran(true);
                QThread::sleep(static_cast<unsigned long>(atlTcpTimeout));
            }
        }
        if (!oktrans) {
            emit calcFinished(false,aUrl,lastError);
            deleteLater();
            return;
        }
    } else if (translationEngine==TE_BINGAPI ||
               translationEngine==TE_YANDEX ||
               translationEngine==TE_GOOGLE_GTX) {
        if (!translateDocument(Uri,aUrl)) {
            QString lastError = tr("Translator initialization error");
            if (tran)
                lastError = tran->getErrorMsg();
            emit calcFinished(false,aUrl,lastError);
            deleteLater();
            return;
        }
    } else {
        if (!calcLocalUrl(Uri,aUrl)) {
            emit calcFinished(false,QString(),tr("ERROR: Url calculation error"));
            deleteLater();
            return;
        }
    }
    emit calcFinished(true,aUrl,QString());
    deleteLater();
}

void CTranslator::abortTranslator()
{
    // Intermediate timer, because sometimes webengine throws segfault from its insides on widget resize event
    auto imt = new QTimer(this);
    imt->setSingleShot(true);
    imt->setInterval(500);
    connect(imt,&QTimer::timeout,this,[this,imt](){
        abortMutex.lock();
        abortFlag=true;
        abortMutex.unlock();
        imt->deleteLater();
    });
    imt->start();
}

void CTranslator::generateHTML(const CHTMLNode &src, QString &html, bool reformat, int depth)
{
    if (src.m_isTag && !src.m_tagName.isEmpty()) {
        html.append(QStringLiteral("<")+src.m_tagName);
        for (const QString &key : qAsConst(src.m_attributesOrder)) {
            const QString val = src.m_attributes.value(key);
            if (!val.contains('"'))
                html.append(QStringLiteral(" %1=\"%2\"").arg(key,val));
            else
                html.append(QStringLiteral(" %1='%2'").arg(key,val));
        }
        html.append(QStringLiteral(">"));
    } else {
        QString txt = src.m_text;
        // fix incorrect nested comments - pixiv
        if (src.m_isComment) {
            if ((txt.count(QStringLiteral("<!--"))>1) || (txt.count(QStringLiteral("-->"))>1))
                txt.clear();
            if ((txt.count(QStringLiteral("<"))>1 || txt.count(QStringLiteral(">"))>1)
                && (txt.count(QStringLiteral("<"))!=txt.count(QStringLiteral(">"))))
                txt.clear();
        }
        html.append(txt);
    }

    for (const CHTMLNode &node : qAsConst(src.m_children))
        generateHTML(node,html,reformat,depth+1);

    html.append(src.m_closingText);
    if (reformat)
        html.append('\n');
}

void CTranslator::replaceLocalHrefs(CHTMLNode& node, const QUrl& baseUrl)
{
    if (node.m_tagName.toLower()==QStringLiteral("a")) {
        if (node.m_attributes.contains(QStringLiteral("href"))) {
            QUrl ref = QUrl(node.m_attributes.value(QStringLiteral("href")));
            if (ref.isRelative())
                node.m_attributes[QStringLiteral("href")]=baseUrl.resolved(ref).toString();
        }
    }

    for (auto &child : node.m_children) {
        replaceLocalHrefs(child,baseUrl);
    }
}

CHTMLNode::~CHTMLNode()
{
    m_children.clear();
    m_attributes.clear();
    m_attributesOrder.clear();
}

CHTMLNode::CHTMLNode(const CHTMLNode &other)
{
    m_text = other.m_text;
    m_tagName = other.m_tagName;
    m_closingText = other.m_closingText;
    m_isTag = other.m_isTag;
    m_isComment = other.m_isComment;

    m_attributes.clear();
    m_attributes = other.m_attributes;

    m_attributesOrder.clear();
    m_attributesOrder = other.m_attributesOrder;

    m_children.clear();
    m_children = other.m_children;
}

CHTMLNode::CHTMLNode(tree<HTML::Node> const & node)
{
    tree<HTML::Node>::iterator it = node.begin();

    it->parseAttributes();

    m_text = it->text();
    m_tagName = it->tagName();
    m_closingText = it->closingText();
    m_isTag = it->isTag();
    m_isComment = it->isComment();

    m_attributes.clear();
    m_attributes = it->attributes();

    m_attributesOrder.clear();
    m_attributesOrder = it->attributesOrder();

    m_children.clear();
    m_children.reserve(static_cast<int>(it.number_of_children()));
    for (unsigned i=0; i<it.number_of_children(); i++ )
        m_children << CHTMLNode(node.child(it,i));
}

CHTMLNode::CHTMLNode(const QString &innerText)
{
    // creates text node
    m_text = innerText;
    m_tagName = innerText;
    m_closingText.clear();
    m_isTag = false;
    m_isComment = false;

    m_attributes.clear();
    m_attributesOrder.clear();
    m_children.clear();
}

CHTMLNode &CHTMLNode::operator=(const CHTMLNode &other)
{
    m_text = other.m_text;
    m_tagName = other.m_tagName;
    m_closingText = other.m_closingText;
    m_isTag = other.m_isTag;
    m_isComment = other.m_isComment;

    m_attributes.clear();
    m_attributes = other.m_attributes;

    m_attributesOrder.clear();
    m_attributesOrder = other.m_attributesOrder;

    m_children.clear();
    m_children = other.m_children;

    return *this;
}

bool CHTMLNode::operator==(const CHTMLNode &s) const
{
    return (m_text == s.m_text);
}

bool CHTMLNode::operator!=(const CHTMLNode &s) const
{
    return !operator==(s);
}

void CHTMLNode::normalize()
{
    int i = 0;
    while (i<(m_children.count()-1)) {
        if (m_children.at(i).isTextNode() &&
                m_children.at(i+1).isTextNode()) {
            if (!m_children.at(i+1).m_text.trimmed().isEmpty())
                m_children[i].m_text += m_children.at(i+1).m_text;
            m_children.removeAt(i+1);
            i = 0;
            continue;
        }
        i++;
    }
}

bool CHTMLNode::isTextNode() const
{
    return (!m_isTag && !m_isComment);
}
