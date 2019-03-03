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
        if (skipTags.contains(node.tagName,Qt::CaseInsensitive)) return;

        if (xmlPass==PXCalculate && !node.isTag && !node.isComment) {
            translateParagraph(node,xmlPass);
        }
        if (xmlPass==PXTranslate && !node.isTag && !node.isComment) {
            if (!translateParagraph(node,xmlPass)) translatorFailed=true;
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
                    metaSrcUrl = QUrl(node.children.at(idx).attributes.value(QStringLiteral("content")).toLower().trimmed());
            }

            if (node.children.at(idx).tagName.toLower()==QStringLiteral("link")) {
                if (node.children.at(idx).attributes.value(QStringLiteral("rel"))
                        .toLower().trimmed()==QStringLiteral("canonical"))
                    metaSrcUrl = QUrl(node.children.at(idx).attributes.value(QStringLiteral("href")).toLower().trimmed());

                if ((node.children.at(idx).attributes.value(QStringLiteral("type"))
                     .toLower().trimmed()==QStringLiteral("text/css")) &&
                        (node.children.at(idx).attributes.value(QStringLiteral("href")).contains(
                             QRegExp(QStringLiteral("blog.*\\.jp.*site.css\\?_="),Qt::CaseInsensitive)) ||
                         (node.children.at(idx).attributes.value(QStringLiteral("href")).contains(
                             QRegExp(QStringLiteral("/css/.*static.css\\?"),Qt::CaseInsensitive)) &&
                          metaSrcUrl.host().contains(QRegExp(QStringLiteral("blog"),Qt::CaseInsensitive))))) {
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
                if (!node.attributes.contains(QStringLiteral("style")))
                    node.attributes[QStringLiteral("style")]=QString();
                if (node.attributes.value(QStringLiteral("style")).isEmpty())
                    node.attributes[QStringLiteral("style")]=QStringLiteral("height:auto;");
                else
                    node.attributes[QStringLiteral("style")]=node.attributes.value(QStringLiteral("style"))+QStringLiteral("; height:auto;");
            }

            // pixiv - unfolding novel pages to one big page
            if (node.attributes.contains(QStringLiteral("class")) &&
                    unhide_classes.contains(node.attributes.value(QStringLiteral("class")).toLower().trimmed())) {
                if (!node.attributes.contains(QStringLiteral("style")))
                    node.attributes[QStringLiteral("style")]=QString();
                if (node.attributes.value(QStringLiteral("style")).isEmpty())
                    node.attributes[QStringLiteral("style")]=QStringLiteral("display:block;");
                else
                    node.attributes[QStringLiteral("style")]=node.attributes.value(QStringLiteral("style"))+QStringLiteral("; display:block;");
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
                for (const CHTMLNode& rnode : qAsConst(node.children.at(idx).children))
                    if (rnode.tagName.toLower()==QStringLiteral("rb"))
                        subnodes << rnode.children;

                for(int si=0;si<subnodes.count();si++)
                    node.children.insert(idx+si+1,subnodes.at(si));

                node.children.removeAt(idx);
                node.normalize();
                idx=0;
                continue;
            }

            // remove floating headers for fc2 and goo blogs
            if (metaSrcUrl.host().contains(QRegExp(QStringLiteral("blog"),Qt::CaseInsensitive)) &&
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
                    imgUrls << src;
            }
        }
        if (node.tagName.toLower()==QStringLiteral("a")) {
            if (node.attributes.contains(QStringLiteral("href"))) {
                QString src = node.attributes.value(QStringLiteral("href")).trimmed();
                QFileInfo fi(src);
                if (acceptedExt.contains(fi.suffix(),Qt::CaseInsensitive))
                    imgUrls << src;
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

    if (translatorFailed) return;

    for(auto &child : node.children) {
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

    QString ssrc = src.text;
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
            src.text = QStringLiteral("<span title=\"%1\">%2</span>")
                       .arg(srctls.join('\n').replace('"','\''),sout);
        else
            src.text = sout;
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
