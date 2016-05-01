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
    if (hostingDir.right(1)!="/") hostingDir=hostingDir+"/";
    if (hostingUrl.right(1)!="/") hostingUrl=hostingUrl+"/";
    atlTcpRetryCount=gSet->settings.atlTcpRetryCount;
    atlTcpTimeout=gSet->settings.atlTcpTimeout;
    forceFontColor=gSet->ui.forceFontColor();
    forcedFontColor=gSet->settings.forcedFontColor;
    useOverrideFont=gSet->ui.useOverrideFont();
    overrideFont=gSet->settings.overrideFont;
    translationMode=gSet->getTranslationMode();
    engine=gSet->settings.translatorEngine;
    srcLanguage=gSet->getSourceLanguageID();
    translateSubSentences=(forceTranSubSentences || gSet->ui.actionTranslateSubSentences->isChecked());
    tran=NULL;
    tranInited=false;
}

CTranslator::~CTranslator()
{
    if (tran!=NULL)
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
    QString wname=QUuid::createUuid().toString().remove(QRegExp("[^a-z,A-Z,0,1-9,-]"))+"."+fi.completeSuffix();

    calculatedUrl="";

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
            if (createdFiles!=NULL) createdFiles->append(wdir+wname);
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

    if (tran==NULL && !tranInited) {
        tran = translatorFactory(this);
        tranInited = true;
    }

    if (tran==NULL || !tran->initTran()) {
        dst=tr("Unable to initialize translation engine.");
        qCritical() << tr("Unable to initialize translation engine.");
        return false;
    }

    dst = "";
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
        dumpPage(token,"2-tree",QByteArray::fromRawData(sst.str().data(),(int)sst.str().size()));
    }

    CHTMLNode doc(tr);

    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"3-converted",doc);

    translatorFailed = false;
    textNodesCnt=0;
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

    if (translatorFailed) {
        dst=tran->getErrorMsg();
    } else {
        examineNode(doc,PXPostprocess);
        generateHTML(doc,dst);

        if (gSet->settings.debugDumpHtml)
            dumpPage(token,"7-finalized",dst);

        tran->doneTran();
    }

    return !translatorFailed;
}

bool CTranslator::documentReparse(const QString &srcUri, QString &dst)
{
    abortMutex.lock();
    abortFlag=false;
    abortMutex.unlock();

    dst = "";
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
        dumpPage(token,"parser-2-tree",QByteArray::fromRawData(sst.str().data(),(int)sst.str().size()));
    }

    CHTMLNode doc(tr);

    if (gSet->settings.debugDumpHtml)
        dumpPage(token,"parser-3-converted",doc);

    translatorFailed=false;
    textNodesCnt=0;
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
        if (node.tagName.toLower()=="meta") {
            if (node.attributes.value("http-equiv").toLower().trimmed()=="content-type")
                node.attributes["content"] = "text/html; charset=UTF-8";
        }

        // Unfold "1% height" divs from blog.jp/livedoor.jp
        int idx = 0;
        while (idx<node.children.count()) {
            if (node.children.at(idx).tagName.toLower()=="link") {
                // their blogs using same site.css
                if ((node.children.at(idx).attributes.value("type").toLower().trimmed()=="text/css") &&
                        (node.children.at(idx).attributes.value("href").contains(
                             QRegExp("blog.*\\.jp.*site.css\\?_=",Qt::CaseInsensitive)))) {
                    CHTMLNode st;
                    st.tagName="style";
                    st.closingText="</style>";
                    st.isTag=true;
                    st.children << CHTMLNode("* { height: auto !important; }");
                    node.children.insert(idx+1,st);
                    break;
                }
            }
            idx++;
        }

        // div processing
        if (node.tagName.toLower()=="div") {

            // unfold compressed divs
            QString sdivst = "";
            if (node.attributes.contains("style"))
                sdivst = node.attributes.value("style");
            if (!sdivst.contains("absolute",Qt::CaseInsensitive)) {
                if (!node.attributes.contains("style"))
                    node.attributes["style"]="";
                if (node.attributes.value("style").isEmpty())
                    node.attributes["style"]="height:auto;";
                else
                    node.attributes["style"]=node.attributes.value("style")+"; height:auto;";
            }

            // pixiv - unfolding novel pages to one big page
            QStringList unhide_classes { "novel-text-wrapper", "novel-pages", "novel-page" };
            if (node.attributes.contains("class") &&
                    unhide_classes.contains(node.attributes.value("class").toLower().trimmed())) {
                if (!node.attributes.contains("style"))
                    node.attributes["style"]="";
                if (node.attributes.value("style").isEmpty())
                    node.attributes["style"]="display:block;";
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
            if (node.children.at(idx).tagName.toLower()=="span" &&
                    node.children.at(idx).attributes.contains("class") &&
                    node.children.at(idx).attributes.value("class")
                    .toLower().trimmed().startsWith("vtoken")) {

                while (!node.children.at(idx).children.isEmpty()) {
                    node.children.insert(idx,node.children[idx].children.takeFirst());
                    idx++;
                }

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
    }

    if (xmlPass==PXPostprocess) {
        // remove duplicated <br>
        int idx=0;
        while(idx<node.children.count()) {
            if (node.children.at(idx).tagName.toLower()=="br") {
                while(idx<(node.children.count()-1) &&
                      node.children.at(idx+1).tagName.toLower()=="br") {
                    node.children.removeAt(idx+1);
                }
            }
            idx++;
        }
    }

    for(int i=0;i<node.children.count();i++) {
        examineNode(node.children[i],xmlPass);
    }
}

bool CTranslator::translateParagraph(CHTMLNode &src, CTranslator::XMLPassMode xmlPass)
{
    if (tran==NULL) return false;

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

void CTranslator::generateHTML(const CHTMLNode &src, QString &html)
{
    if (src.isTag && !src.tagName.isEmpty()) {
        html.append("<"+src.tagName);
        for (int i=0;i<src.attributesOrder.count();i++) {
            QString key = src.attributesOrder.at(i);
            QString val = src.attributes.value(key);
            if (!val.contains("\""))
                html.append(QString(" %1=\"%2\"").arg(key,val));
            else
                html.append(QString(" %1='%2'").arg(key,val));
        }
        html.append(">");
    } else
        html.append(src.text);

    for (int i=0; i<src.children.count(); i++ )
        generateHTML(src.children.at(i),html);

    html.append(src.closingText);
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
    if (translationEngine==TE_ATLAS) {
        bool oktrans = false;
        QString lastAtlasError;
        lastAtlasError.clear();
        for (int i=0;i<atlTcpRetryCount;i++) {
            if (translateDocument(Uri,aUrl)) {
                oktrans = true;
                break;
            } else
                lastAtlasError = tran->getErrorMsg();
            QThread::sleep(atlTcpTimeout);
            if (tran!=NULL)
                tran->doneTran(true);
        }
        if (!oktrans) {
            emit calcFinished(false,lastAtlasError);
            deleteLater();
            return;
        }
    } else if (translationEngine==TE_BINGAPI ||
               translationEngine==TE_YANDEX) {
        if (!translateDocument(Uri,aUrl)) {
            emit calcFinished(false,tran->getErrorMsg());
            deleteLater();
            return;
        }
    } else {
        if (!calcLocalUrl(Uri,aUrl)) {
            emit calcFinished(false,"");
            deleteLater();
            return;
        }
    }
    emit calcFinished(true,aUrl);
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
