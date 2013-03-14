#include <QProcess>
#include "calcthread.h"
#include "snviewer.h"
#include "genericfuncs.h"

CCalcThread::CCalcThread(QObject* parent, QString aUri, CWaitDlg *aWaitDlg)
    : QThread(parent)
{
    waitDlg = aWaitDlg;
    hostingDir=gSet->hostingDir;
    hostingUrl=gSet->hostingUrl;
    createdFiles=&(gSet->createdFiles);
    Uri=aUri;
    useSCP=gSet->useScp;
    scpParams=gSet->scpParams;
    scpHost=gSet->scpHost;
    atlHost=gSet->atlHost;
    atlPort=gSet->atlPort;
    translationEngine=gSet->translatorEngine;
    if (hostingDir.right(1)!="/") hostingDir=hostingDir+"/";
    if (hostingUrl.right(1)!="/") hostingUrl=hostingUrl+"/";
}

bool CCalcThread::calcLocalUrl(const QString& aUri, QString& calculatedUrl)
{
    QString wdir = hostingDir;
    if (wdir.isEmpty() || hostingUrl.isEmpty() || hostingUrl.contains("about:blank",Qt::CaseInsensitive)) return false;
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

void CCalcThread::examineXMLNode(QDomNode node)
{
    if (atlasSlipped) return;
    abortMutex.lock();
    if (atlasToAbort) {
        abortMutex.unlock();
        return;
    }
    abortMutex.unlock();

    if (xmlPass==PXTranslate || xmlPass==PXCalculate) {
        if (node.isText() && node.parentNode().nodeName().toLower()=="title") return;

        if (xmlPass==PXCalculate && node.isText()) { // modify node
			QDomNode nd = node.ownerDocument().createElement("p");
            nd.appendChild(node.ownerDocument().createTextNode(node.nodeValue()));
            QDomAttr flg = node.ownerDocument().createAttribute("jpreader_flag");
            flg.setNodeValue("on");
            nd.attributes().setNamedItem(flg);
            translateParagraph(nd);
            node.parentNode().replaceChild(nd,node);
        }
        if (xmlPass==PXTranslate && node.attributes().contains("jpreader_flag")) {
            node.attributes().removeNamedItem("jpreader_flag");
            if (!translateParagraph(node)) atlasSlipped=true;
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
            if (deniedNodes.contains(node.childNodes().at(ridx).nodeName(),Qt::CaseInsensitive))
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
                QDomAttr divst = node.ownerDocument().createAttribute("style");
                divst.setNodeValue("height:auto;");
                node.attributes().setNamedItem(divst);
            }
        }

        // pixiv stylesheet fix for novels viewer
        if (node.nodeName().toLower()=="div") {
            if (!node.attributes().namedItem("class").isNull() &&
                    node.attributes().namedItem("class").nodeValue().toLower()=="works_display") {
                if (node.attributes().namedItem("style").isNull())
                    node.attributes().setNamedItem(node.ownerDocument().createAttribute("style"));
                node.attributes().namedItem("style").setNodeValue("text-align: left;");
            }
        }

		// remove <pre> tags
		if (node.isElement() && node.nodeName().toLower()=="pre") {
			node.toElement().setTagName("p");
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
        examineXMLNode(node.childNodes().at(i));
    }
}

bool CCalcThread::translateWithAtlas(const QString &srcUri, QString &dst)
{
    abortMutex.lock();
    atlasToAbort=false;
    abortMutex.unlock();

    if (!atlas.initTran(atlHost,atlPort)) {
        dst=tr("Unable to initialize ATLAS.");
        qDebug() << tr("Unable to initialize ATLAS.");
        return false;
    }
    connect(waitDlg->abortBtn,SIGNAL(clicked()),this,SLOT(abortAtlas()));

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
        atlas.doneTran();
        qDebug() << tr("HTML to XML beautification error");
        qDebug() << errMsg << "at line" << errLine << ":" << errCol;
        return false;
    }

    atlasSlipped=false;
    textNodesCnt=0;
    xmlPass=PXPreprocess;
    examineXMLNode(doc);
    //qDebug() << doc.toString();

    xmlPass=PXCalculate;
    examineXMLNode(doc);
    //qDebug() << doc.toString();

    textNodesProgress=0;
    xmlPass=PXTranslate;
    examineXMLNode(doc);
    //qDebug() << doc.toString();

    if (atlasSlipped)
        dst="ERROR:ATLAS_SLIPPED";
    else {
        xmlPass=PXPostprocess;
        examineXMLNode(doc);

        dst = doc.toString();

        atlas.doneTran();
    }

/*    QFile f(QDir::home().absoluteFilePath("a.html"));
    f.open(QIODevice::WriteOnly);
    f.write(doc.toByteArray());
    f.close();*/

    disconnect(waitDlg->abortBtn,0,this,0);
    return true;
}

bool CCalcThread::translateParagraph(QDomNode src)
{
    int baseProgress = 0;
    if (textNodesCnt>0) {
        baseProgress = 100*textNodesProgress/textNodesCnt;
    }
    QMetaObject::invokeMethod(waitDlg,"setProgressValue",Q_ARG(int, baseProgress));

    QString ssrc = src.firstChild().nodeValue();
    ssrc = ssrc.replace("\r\n","\n");
    ssrc = ssrc.replace('\r','\n');
    ssrc = ssrc.replace(QRegExp("\n{2,}"),"\n");
    QStringList sl = ssrc.split('\n',QString::SkipEmptyParts);

    if (xmlPass==PXTranslate) src.removeChild(src.firstChild());
    for(int i=0;i<sl.count();i++) {
        abortMutex.lock();
        if (atlasToAbort) {
            abortMutex.unlock();
            src.appendChild(src.ownerDocument().createTextNode(tr("ATLAS: Translation thread aborted.")));
            break;
        }
        abortMutex.unlock();

        if (xmlPass!=PXTranslate) {
            textNodesCnt++;
        } else {
			QDomNode p = src.ownerDocument().createElement("p");
			p.appendChild(p.ownerDocument().createTextNode(sl[i]));
            p.appendChild(p.ownerDocument().createElement("br"));

            QString t = atlas.tranString(sl[i]);
            if (t.contains("ERROR:ATLAS_SLIPPED")) {
                atlas.doneTran();
                src.appendChild(src.ownerDocument().createTextNode(t));
                return false;
            } else {
                if (gSet->useOverrideFont || gSet->forceFontColor) {
                    QDomNode dp = p.ownerDocument().createElement("div");
                    dp.attributes().setNamedItem(p.ownerDocument().createAttribute("style"));
                    QString dstyle;
                    if (gSet->useOverrideFont)
                        dstyle+=tr("font-family: %1; font-size: %2pt;").arg(
                                    gSet->overrideFont.family()).arg(
                                    gSet->overrideFont.pointSize());
                    if (gSet->forceFontColor)
                        dstyle+=tr("color: %1;").arg(gSet->forcedFontColor.name());
                    dp.attributes().namedItem("style").setNodeValue(dstyle);
                    dp.appendChild(p.ownerDocument().createTextNode(t));
                    p.appendChild(dp);
                } else {
                    p.appendChild(p.ownerDocument().createTextNode(t));
                }
            }

            src.appendChild(p);

            if (textNodesCnt>0 && i%5==0) {
                int pr = 100*textNodesProgress/textNodesCnt;
                QMetaObject::invokeMethod(waitDlg,"setProgressValue",Q_ARG(int, pr));
            }
            textNodesProgress++;
        }
    }
    return true;
}

void CCalcThread::run()
{
    QString aUrl;
    if (translationEngine==TE_ATLAS) {
        if (!translateWithAtlas(Uri,aUrl)) {
            emit calcFinished(false,"");
            return;
        }
    } else {
        if (!calcLocalUrl(Uri,aUrl)) {
            emit calcFinished(false,"");
            return;
        }
    }
    emit calcFinished(true,aUrl);
}

void CCalcThread::abortAtlas()
{
    abortMutex.lock();
    atlasToAbort=true;
    abortMutex.unlock();
}
