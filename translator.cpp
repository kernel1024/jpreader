#include <QProcess>
#include "translator.h"
#include "snviewer.h"
#include "genericfuncs.h"

CTranslator::CTranslator(QObject* parent, QString aUri, CSnWaitCtl *aWaitDlg)
    : QObject(parent)
{
    waitDlg = aWaitDlg;
    hostingDir=gSet->hostingDir;
    hostingUrl=gSet->hostingUrl;
    createdFiles=&(gSet->createdFiles);
    Uri=aUri;
    useSCP=gSet->useScp;
    scpParams=gSet->scpParams;
    scpHost=gSet->scpHost;
    translationEngine=gSet->translatorEngine;
    if (hostingDir.right(1)!="/") hostingDir=hostingDir+"/";
    if (hostingUrl.right(1)!="/") hostingUrl=hostingUrl+"/";
    atlTcpRetryCount=gSet->atlTcpRetryCount;
    atlTcpTimeout=gSet->atlTcpTimeout;
    forceFontColor=gSet->forceFontColor();
    forcedFontColor=gSet->forcedFontColor;
    useOverrideFont=gSet->useOverrideFont();
    overrideFont=gSet->overrideFont;
    translationMode=gSet->getTranslationMode();
    engine=gSet->translatorEngine;
    srcLanguage=gSet->getSourceLanguageID();
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
        scpp << QString("%1:%2%3").arg(scpHost).arg(wdir).arg(wname);
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

void CTranslator::examineXMLNode(QDomNode node, XMLPassMode xmlPass)
{
    if (translatorFailed) return;
    abortMutex.lock();
    if (abortFlag) {
        abortMutex.unlock();
        return;
    }
    abortMutex.unlock();

    QApplication::processEvents();

    if ((xmlPass==PXTranslate || xmlPass==PXCalculate) && tranInited) {
        if (node.isText() && node.parentNode().nodeName().toLower()=="title") return;

        if (xmlPass==PXCalculate && node.isText()) { // modify node
            QDomNode nd = node.ownerDocument().createElement("span");
            nd.appendChild(node.ownerDocument().createTextNode(node.nodeValue()));
            QDomAttr flg = node.ownerDocument().createAttribute("jpreader_flag");
            flg.setNodeValue("on");
            nd.attributes().setNamedItem(flg);
            translateParagraph(nd,xmlPass);
            node.parentNode().replaceChild(nd,node);
        }
        if (xmlPass==PXTranslate && node.attributes().contains("jpreader_flag")) {
            node.attributes().removeNamedItem("jpreader_flag");
            if (!translateParagraph(node,xmlPass)) translatorFailed=true;
        }
    }

	if (xmlPass==PXPreprocess) {
        if (node.isElement() && node.nodeName().toLower()=="meta") {
            if (!node.attributes().namedItem("http-equiv").isNull() &&
                    node.attributes().namedItem("http-equiv").nodeValue().toLower()=="content-type") {
                if (!node.attributes().namedItem("content").isNull())
                    node.attributes().namedItem("content").setNodeValue("text/html; charset=UTF-8");
            }
        }

		// cleanup iframes, objects and other unusable tags
		QStringList deniedNodes;
        deniedNodes << "object" << "iframe" << "noscript" << "script";
        int ridx = 0;
        while (ridx<node.childNodes().count()) {
            if (deniedNodes.contains(node.childNodes().at(ridx).nodeName(),Qt::CaseInsensitive) &&
                    node.childNodes().at(ridx).isElement())
                node.removeChild(node.childNodes().at(ridx));
            else
                ridx++;
		}

		// fix script and styles comment blocks
		if (node.nodeName().toLower()=="script" || node.nodeName().toLower()=="style") {
			if (!node.firstChild().toText().isNull()) {
				QString b = node.firstChild().toText().nodeValue();
				b.remove("<!--"); b.remove("-->");
				b = b.simplified();
				node.removeChild(node.firstChild());
				node.appendChild(node.ownerDocument().createComment(b));
			}
		}

        // expand null-sized unclosed container tags into small paragraphs
        QStringList containerTags;
        containerTags << "b" << "center" << "cite" << "code" <<
                         "dd" << "del" << "dir" << "div" << "dl" << "dt" << "em" << "font" << "form" <<
                         "h1" << "h2" << "h3" << "h4" << "h5" << "h6" << "i" << "ins" << "label" << " legend" <<
                         "li" << "map" << "menu" << "p" << "pre" << "q" << "s" << "select" <<
                         "small" << "span" << "strike" << "strong" << "style" << "table" << "tbody" <<
                         "textarea" << "title" <<"tr" << "tt" << "u" << "ul";
        if (containerTags.contains(node.nodeName().toLower()) && node.childNodes().isEmpty()) {
			node.appendChild(node.ownerDocument().createTextNode(" "));
		}

        // unfold compressed divs
        if (node.nodeName().toLower()=="div") {
            QString sdivst = "";
            if (!node.attributes().namedItem("style").isNull())
                sdivst = node.attributes().namedItem("style").nodeValue().toLower();

            if (!sdivst.contains("absolute",Qt::CaseInsensitive)) {
                if (node.attributes().namedItem("style").isNull())
                    node.attributes().setNamedItem(node.ownerDocument().createAttribute("style"));
                if (node.attributes().namedItem("style").nodeValue().isEmpty())
                    node.attributes().namedItem("style").setNodeValue("height:auto;");
                else
                    node.attributes().namedItem("style").setNodeValue(
                                node.attributes().namedItem("style").nodeValue()+"; height:auto;");
            }
        }

        // pixiv stylesheet fix for novels viewer
        if (node.nodeName().toLower()=="div") {
            if (!node.attributes().namedItem("class").isNull()) {
                if (node.attributes().namedItem("class").nodeValue().toLower()=="works_display") {
                    if (node.attributes().namedItem("style").isNull())
                        node.attributes().setNamedItem(node.ownerDocument().createAttribute("style"));
                    node.attributes().namedItem("style").setNodeValue("text-align: left;");
                }

                // hide comments popup div
                if (node.attributes().namedItem("class").nodeValue().toLower().contains("-modal")) {
                    if (node.attributes().namedItem("style").isNull())
                        node.attributes().setNamedItem(node.ownerDocument().createAttribute("style"));
                    if (node.attributes().namedItem("style").nodeValue().isEmpty())
                        node.attributes().namedItem("style").setNodeValue("position: static;");
                    else
                        node.attributes().namedItem("style").setNodeValue(
                                    node.attributes().namedItem("style").nodeValue()+"; position: static;");
                }

                // Style fix for 2015/05..2015/07 pixiv novel viewer update
                if ((node.attributes().namedItem("class").nodeValue().toLower()=="novel-text-wrapper") ||
                        (node.attributes().namedItem("class").nodeValue().toLower()=="novel-pages")) {
                    if (node.attributes().namedItem("style").isNull())
                        node.attributes().setNamedItem(node.ownerDocument().createAttribute("style"));
                    if (node.attributes().namedItem("style").nodeValue().isEmpty())
                        node.attributes().namedItem("style").setNodeValue("display:block;");
                    else
                        node.attributes().namedItem("style").setNodeValue(
                                    node.attributes().namedItem("style").nodeValue()+"; display:block;");
                }
            }
        }

        // Style fix for 2015/05 pixiv novel viewer update - advanced workarounds
        int idx=0;
        while(idx<node.childNodes().count()) {
            // Remove 'vtoken' inline span
            QDomNode span = node.childNodes().item(idx);
            if (span.nodeName().toLower()=="span" &&
                    !span.attributes().namedItem("class").isNull() &&
                    span.attributes().namedItem("class").nodeValue().toLower().startsWith("vtoken")) {

                while(!span.childNodes().isEmpty())
                    node.insertBefore(span.removeChild(span.firstChild()),span);

                node.removeChild(span);
                node.normalize();
                idx=0;
            }
            idx++;
        }

		// remove <pre> tags
		if (node.isElement() && node.nodeName().toLower()=="pre") {
            node.toElement().setTagName("span");
		}

    }
	if (xmlPass==PXPostprocess) {
        // remove duplicated <br>
        int idx=0;
        while(idx<node.childNodes().count()) {
            if (node.childNodes().at(idx).nodeName().toLower()=="br") {
                while(!node.childNodes().at(idx+1).isNull() &&
                      node.childNodes().at(idx+1).nodeName().toLower()=="br") {
                    node.removeChild(node.childNodes().at(idx+1));
                }
            }
            idx++;
        }
    }


    for(int i=0;i<node.childNodes().count();i++) {
        examineXMLNode(node.childNodes().at(i),xmlPass);
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

    // convert source HTML to XML
	QString src = srcUri.trimmed();
	src = src.remove(0,src.indexOf("<html",Qt::CaseInsensitive));
    QByteArray xmls = XMLizeHTML(src.toUtf8(),"UTF-8");

    // load into DOM
    QDomDocument doc;
    QString errMsg; int errLine, errCol;
    if (!doc.setContent(xmls,&errMsg,&errLine,&errCol)) {
        tran->doneTran();
        qCritical() << tr("HTML to XML beautification error");
        qCritical() << errMsg << "at line" << errLine << ":" << errCol;
        return false;
    }

    translatorFailed=false;
    textNodesCnt=0;
    XMLPassMode xmlPass=PXPreprocess;
    examineXMLNode(doc,xmlPass);
    //qInfo() << doc.toString();

    xmlPass=PXCalculate;
    examineXMLNode(doc,xmlPass);
    //qInfo() << doc.toString();

    textNodesProgress=0;
    xmlPass=PXTranslate;
    examineXMLNode(doc,xmlPass);
    //qInfo() << doc.toString();

    if (translatorFailed) {
        dst=tran->getErrorMsg();
    } else {
        xmlPass=PXPostprocess;
        examineXMLNode(doc,xmlPass);

        dst = doc.toString(-1);

        tran->doneTran();
    }

/*    QFile f(QDir::home().absoluteFilePath("a.html"));
    f.open(QIODevice::WriteOnly);
    f.write(doc.toByteArray());
    f.close();*/

    return !translatorFailed;
}

bool CTranslator::documentToXML(const QString &srcUri, QString &dst)
{
    abortMutex.lock();
    abortFlag=false;
    abortMutex.unlock();

    dst = "";
    if (srcUri.isEmpty()) return false;

    // convert source HTML to XML
    QString src = srcUri.trimmed();
    src = src.remove(0,src.indexOf("<html",Qt::CaseInsensitive));
    QByteArray xmls = XMLizeHTML(src.toUtf8(),"UTF-8");

    // load into DOM
    QDomDocument doc;
    QString errMsg; int errLine, errCol;
    if (!doc.setContent(xmls,&errMsg,&errLine,&errCol)) {
        qCritical() << tr("HTML to XML beautification error");
        qCritical() << errMsg << "at line" << errLine << ":" << errCol;
        return false;
    }

    translatorFailed=false;
    textNodesCnt=0;
    XMLPassMode xmlPass=PXPreprocess;
    examineXMLNode(doc,xmlPass);

    xmlPass=PXCalculate;
    examineXMLNode(doc,xmlPass);

    textNodesProgress=0;
    xmlPass=PXTranslate;
    examineXMLNode(doc,xmlPass);

    dst = doc.toString(-1);

    return true;
}

bool CTranslator::translateParagraph(QDomNode src, XMLPassMode xmlPass)
{
    if (tran==NULL) return false;

    int baseProgress = 0;
    if (textNodesCnt>0) {
        baseProgress = 100*textNodesProgress/textNodesCnt;
    }
    QMetaObject::invokeMethod(waitDlg,"setProgressValue",Qt::QueuedConnection,Q_ARG(int, baseProgress));

    QString ssrc = src.firstChild().nodeValue();
    ssrc = ssrc.replace("\r\n","\n");
    ssrc = ssrc.replace('\r','\n');
    ssrc = ssrc.replace(QRegExp("\n{2,}"),"\n");
    QStringList sl = ssrc.split('\n',QString::SkipEmptyParts);

    if (xmlPass==PXTranslate) src.removeChild(src.firstChild());
    for(int i=0;i<sl.count();i++) {
        abortMutex.lock();
        if (abortFlag) {
            abortMutex.unlock();
            src.appendChild(src.ownerDocument().createTextNode(tr("ATLAS: Translation thread aborted.")));
            break;
        }
        abortMutex.unlock();

        if (xmlPass!=PXTranslate) {
            textNodesCnt++;
        } else {
            QString srct = sl[i];
            QString t = tran->tranString(sl[i]);
            if (translationMode==TM_TOOLTIP) {
                QString ts = srct;
                srct = t;
                t = ts;
            }

            if (translationMode==TM_ADDITIVE) {
                src.appendChild(src.ownerDocument().createTextNode(srct));
                src.appendChild(src.ownerDocument().createElement("br"));
            }
            if (!tran->getErrorMsg().isEmpty()) {
                tran->doneTran();
                src.appendChild(src.ownerDocument().createTextNode(t));
                return false;
            } else {
                if (useOverrideFont || forceFontColor) {
                    QDomNode dp = src.ownerDocument().createElement("span");
                    dp.attributes().setNamedItem(src.ownerDocument().createAttribute("style"));
                    QString dstyle;
                    if (useOverrideFont)
                        dstyle+=tr("font-family: %1; font-size: %2pt;").arg(
                                    overrideFont.family()).arg(
                                    overrideFont.pointSize());
                    if (forceFontColor)
                        dstyle+=tr("color: %1;").arg(forcedFontColor.name());
                    dp.attributes().namedItem("style").setNodeValue(dstyle);
                    dp.appendChild(src.ownerDocument().createTextNode(t));
                    src.appendChild(dp);
                } else {
                    src.appendChild(src.ownerDocument().createTextNode(t));
                }
                if ((translationMode==TM_OVERWRITING) || (translationMode==TM_TOOLTIP)) {
                    src.attributes().setNamedItem(src.ownerDocument().createAttribute("title"));
                    src.attributes().namedItem("title").setNodeValue(srct);
                } else
                    src.appendChild(src.ownerDocument().createElement("br"));
            }

            if (textNodesCnt>0 && i%5==0) {
                int pr = 100*textNodesProgress/textNodesCnt;
                QMetaObject::invokeMethod(waitDlg,"setProgressValue",Qt::QueuedConnection,Q_ARG(int, pr));
            }
            textNodesProgress++;
        }
    }
    return true;
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
    abortMutex.lock();
    abortFlag=true;
    abortMutex.unlock();
}
