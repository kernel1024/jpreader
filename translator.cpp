#include <QProcess>
#include <QBuffer>
#include "translator.h"
#include "snviewer.h"
#include "genericfuncs.h"
#include <sstream>

using namespace htmlcxx;

CTranslator::CTranslator(QObject* parent, QString aUri, bool forceTranSubSentences)
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
    if (tran!=nullptr)
        tran->deleteLater();
}

bool CTranslator::calcLocalUrl(const QString& aUri, QString& calculatedUrl)
{
    QString wdir = hostingDir;
    if (wdir.isEmpty()
            || hostingUrl.isEmpty()
            || !hostingUrl.startsWith("http",Qt::CaseInsensitive)) return false;
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
        scpp << QString("%1:%2%3").arg(scpHost,wdir,wname);
        r=(QProcess::execute("scp",scpp)==0);
        if (r) calculatedUrl=hostingUrl+wname;
        return r;
    } else {
        r=QFile(filename).copy(wdir+wname);
        if (r) {
            if (createdFiles!=nullptr) createdFiles->append(wdir+wname);
            calculatedUrl=hostingUrl+wname;
            b = true;
        } else
            b = false;
        return b;
    }
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
        dumpPage(token,"1-source",src);
    src = src.remove(0,src.indexOf("<html",Qt::CaseInsensitive));

    HTML::ParserDom parser;
    parser.parse(src);

    tree<HTML::Node> tr = parser.getTree();

    if (gSet->settings.debugDumpHtml) {
        std::stringstream sst;
        sst << tr;
        dumpPage(token,"2-tree",QByteArray::fromRawData(sst.str().data(),
                                                        static_cast<int>(sst.str().size())));
    }

    CHTMLNode doc(tr);

    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"3-converted",doc);

    translatorFailed = false;
    textNodesCnt=0;
    metaSrcUrl.clear();
    examineNode(doc,PXPreprocess);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"4-preprocessed",doc);

    examineNode(doc,PXCalculate);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"5-calculated",doc);

    textNodesProgress=0;
    examineNode(doc,PXTranslate);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"6-translated",doc);

    examineNode(doc,PXPostprocess);
    generateHTML(doc,dst);

    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"7-finalized",dst);

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
        dumpPage(token,"parser-1-source",src);
    src = src.remove(0,src.indexOf("<html",Qt::CaseInsensitive));

    HTML::ParserDom parser;
    parser.parse(src);

    tree<HTML::Node> tr = parser.getTree();

    if (gSet->settings.debugDumpHtml) {
        std::stringstream sst;
        sst << tr;
        dumpPage(token,"parser-2-tree",QByteArray::fromRawData(sst.str().data(),
                                                               static_cast<int>(sst.str().size())));
    }

    CHTMLNode doc(tr);

    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"parser-3-converted",doc);

    translatorFailed=false;
    textNodesCnt=0;
    metaSrcUrl.clear();
    examineNode(doc,PXPreprocess);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"parser-4-preprocessed",doc);

    examineNode(doc,PXCalculate);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"parser-5-calculated",doc);

    textNodesProgress=0;
    examineNode(doc,PXTranslate);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"parser-6-translated",doc);

    generateHTML(doc,dst);
    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"parser-7-finalized",dst);

    return true;
}

QStringList CTranslator::getImgUrls()
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

    QStringList skipTags;
    skipTags << "style" << "title";

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
        if (node.tagName.toLower()==QLatin1String("meta")) {
            if (node.attributes.value("http-equiv").toLower().trimmed()==QLatin1String("content-type"))
                node.attributes["content"] = QLatin1String("text/html; charset=UTF-8");
        }

        // Unfold "1% height" divs from blog.jp/livedoor.jp
        // their blogs using same site.css
        // Also for blog.goo.ne.jp (same CSS as static.css)
        int idx = 0;
        while (idx<node.children.count()) {
            if (node.children.at(idx).tagName.toLower()==QLatin1String("meta")) {
                if (node.children.at(idx).attributes.value("property")
                        .toLower().trimmed()==QLatin1String("og:url"))
                    metaSrcUrl = QUrl(node.children.at(idx).attributes.value("content").toLower().trimmed());
            }

            if (node.children.at(idx).tagName.toLower()==QLatin1String("link")) {
                if (node.children.at(idx).attributes.value("rel")
                        .toLower().trimmed()==QLatin1String("canonical"))
                    metaSrcUrl = QUrl(node.children.at(idx).attributes.value("href").toLower().trimmed());

                if ((node.children.at(idx).attributes.value("type")
                     .toLower().trimmed()==QLatin1String("text/css")) &&
                        (node.children.at(idx).attributes.value("href").contains(
                             QRegExp("blog.*\\.jp.*site.css\\?_=",Qt::CaseInsensitive)) ||
                         (node.children.at(idx).attributes.value("href").contains(
                             QRegExp("/css/.*static.css\\?",Qt::CaseInsensitive)) &&
                          metaSrcUrl.host().contains(QRegExp("blog",Qt::CaseInsensitive))))) {
                    CHTMLNode st;
                    st.tagName=QLatin1String("style");
                    st.closingText=QLatin1String("</style>");
                    st.isTag=true;
                    st.children << CHTMLNode("* { height: auto !important; }");
                    node.children.insert(idx+1,st);
                    break;
                }
            }
            idx++;
        }

        // div processing
        if (node.tagName.toLower()==QLatin1String("div")) {

            // unfold compressed divs
            QString sdivst;
            if (node.attributes.contains("style"))
                sdivst = node.attributes.value("style");
            if (!sdivst.contains("absolute",Qt::CaseInsensitive)) {
                if (!node.attributes.contains("style"))
                    node.attributes["style"]=QString();
                if (node.attributes.value("style").isEmpty())
                    node.attributes["style"]=QLatin1String("height:auto;");
                else
                    node.attributes["style"]=node.attributes.value("style")+"; height:auto;";
            }

            // pixiv - unfolding novel pages to one big page
            QStringList unhide_classes { "novel-text-wrapper", "novel-pages", "novel-page" };
            if (node.attributes.contains("class") &&
                    unhide_classes.contains(node.attributes.value("class").toLower().trimmed())) {
                if (!node.attributes.contains("style"))
                    node.attributes["style"]=QString();
                if (node.attributes.value("style").isEmpty())
                    node.attributes["style"]=QLatin1String("display:block;");
                else
                    node.attributes["style"]=node.attributes.value("style")+"; display:block;";
            }
        }

        idx=0;
        QStringList denyTags;
        denyTags << "script" << "noscript" << "object" << "iframe";
        while(idx<node.children.count()) {
            // Style fix for 2015/05 pixiv novel viewer update - advanced workarounds
            // Remove 'vtoken' inline span
            if (node.children.at(idx).tagName.toLower()==QLatin1String("span") &&
                    node.children.at(idx).attributes.contains("class") &&
                    node.children.at(idx).attributes.value("class")
                    .toLower().trimmed().startsWith("vtoken")) {

                QList<CHTMLNode> subnodes = node.children.at(idx).children;
                for(int si=0;si<subnodes.count();si++)
                    node.children.insert(idx+si+1,subnodes.at(si));

                node.children.removeAt(idx);
                node.normalize();
                idx=0;
                continue;
            }

            // remove ruby annotation (superscript), unfold main text block
            if (node.children.at(idx).tagName.toLower()==QLatin1String("ruby")) {
                QList<CHTMLNode> subnodes;
                foreach (const CHTMLNode& rnode, node.children.at(idx).children)
                    if (rnode.tagName.toLower()==QLatin1String("rb"))
                        subnodes << rnode.children;

                for(int si=0;si<subnodes.count();si++)
                    node.children.insert(idx+si+1,subnodes.at(si));

                node.children.removeAt(idx);
                node.normalize();
                idx=0;
                continue;
            }

            QStringList denyDivs;
            denyDivs << "sh_fc2blogheadbar" << "fc2_bottom_bnr" << "global-header";
            // remove floating headers for fc2 and goo blogs
            if (metaSrcUrl.host().contains(QRegExp("blog",Qt::CaseInsensitive)) &&
                    node.children.at(idx).tagName.toLower()==QLatin1String("div") &&
                    node.children.at(idx).attributes.contains("id") &&
                    denyDivs.contains(node.children.at(idx).attributes.value("id"),
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
        if (node.tagName.toLower()==QLatin1String("img")) {
            if (node.attributes.contains("src")) {
                QString src = node.attributes.value("src").trimmed();
                if (!src.isEmpty())
                    imgUrls << src;
            }
        }
    }

    if (xmlPass==PXPostprocess) {
        // remove duplicated <br>
        int idx=0;
        while(idx<node.children.count()) {
            if (node.children.at(idx).tagName.toLower()==QLatin1String("br")) {
                while(idx<(node.children.count()-1) &&
                      node.children.at(idx+1).tagName.toLower()==QLatin1String("br")) {
                    node.children.removeAt(idx+1);
                }
            }
            idx++;
        }
    }

    if (translatorFailed) return;

    for(int i=0;i<node.children.count();i++) {
        examineNode(node.children[i],xmlPass);
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
    ssrc = ssrc.replace("\r\n","\n");
    ssrc = ssrc.replace('\r','\n');
    ssrc = ssrc.replace(QRegExp("\n{2,}"),"\n");
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
                sout += "\n";
            else {
                QString t = QString();

                QString ttest = srct;
                bool noText = true;
                ttest.remove(QRegExp("&\\w+;"));
                for (int j=0;j<ttest.length();j++) {
                    if (ttest.at(j).isLetter()) {
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
                    sout += srct + "<br/>";

                if (!tran->getErrorMsg().isEmpty()) {
                    tran->doneTran();
                    sout += t;
                    failure = true;
                    break;
                } else {
                    if (useOverrideFont || forceFontColor) {
                        QString dstyle;
                        if (useOverrideFont)
                            dstyle+=tr("font-family: %1; font-size: %2pt;").arg(
                                        overrideFont.family()).arg(
                                        overrideFont.pointSize());
                        if (forceFontColor)
                            dstyle+=tr("color: %1;").arg(forcedFontColor.name());

                        sout += QString("<span style=\"%1\">%2</span>").arg(dstyle,t);
                    } else
                        sout += t;

                    if (translationMode==TM_ADDITIVE)
                        sout += "<br/>\n";
                    else
                        srctls << srct;
                }
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
            src.text = QString("<span title=\"%1\">%2</span>")
                       .arg(srctls.join("\n").replace('"','\''),sout);
        else
            src.text = sout;
    }

    if (failure)
        return false;


    return true;

}

void CTranslator::dumpPage(const QUuid &token, const QString &suffix, const QString &page)
{
    QString fname = getTmpDir() + QDir::separator() + token.toString() + "-" + suffix + ".html";
    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to create dump file " << fname;
        return;
    }
    f.write(page.toUtf8());
    f.close();
}

void CTranslator::dumpPage(const QUuid &token, const QString &suffix, const CHTMLNode &page)
{
    QString fname = getTmpDir() + QDir::separator() + token.toString() + "-" + suffix + ".html";
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

void CTranslator::dumpPage(const QUuid &token, const QString &suffix, const QByteArray &page)
{
    QString fname = getTmpDir() + QDir::separator() + token.toString() + "-" + suffix + ".html";
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
                } else if (tran!=nullptr)
                    lastError = tran->getErrorMsg();
                else
                    lastError = tr("ATLAS translator failed.");
                if (tran!=nullptr)
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
            if (tran!=nullptr)
                lastError = tran->getErrorMsg();
            emit calcFinished(false,aUrl,lastError);
            deleteLater();
            return;
        }
    } else {
        if (!calcLocalUrl(Uri,aUrl)) {
            emit calcFinished(false,QString(),QString("ERROR: Url calculation error"));
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
    QTimer* imt = new QTimer(this);
    imt->setSingleShot(true);
    imt->setInterval(500);
    connect(imt,&QTimer::timeout,[this,imt](){
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
