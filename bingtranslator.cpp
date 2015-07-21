#include <QUrl>

#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegExp>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QUrlQuery>

#include "bingtranslator.h"
#include "globalcontrol.h"

CBingTranslator::CBingTranslator(QObject *parent, const QString &bingID, const QString &bingKey)
    : CAbstractTranslator(parent)
{
    clientID = bingID;
    clientKey = bingKey;
    authHeader.clear();
    srcLang.clear();
    nam = NULL;
}

CBingTranslator::~CBingTranslator()
{

}

bool CBingTranslator::waitForReply(QNetworkReply *reply)
{
    QEventLoop eventLoop;
    QTimer *timer = new QTimer(this);

    connect(reply,SIGNAL(finished()), &eventLoop, SLOT(quit()));
    connect(timer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
    timer->setSingleShot(true);
    timer->start(30000);

    eventLoop.exec();

    if (timer != NULL)
    {
        timer->stop();
        timer->deleteLater();
    }

    return reply->isFinished() && reply->bytesAvailable()>0;
}

bool CBingTranslator::initTran()
{
    if (nam==NULL)
        nam=new QNetworkAccessManager(this);

    QUrlQuery postData;
    postData.addQueryItem("grant_type","client_credentials");
    postData.addQueryItem("client_id",QUrl::toPercentEncoding(clientID));
    postData.addQueryItem("client_secret",QUrl::toPercentEncoding(clientKey));
    postData.addQueryItem("scope",QUrl::toPercentEncoding("http://api.microsofttranslator.com"));
    QByteArray rqdata = postData.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest rq(QUrl("https://datamarket.accesscontrol.windows.net/v2/OAuth2-13"));
    rq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QNetworkReply *rpl = nam->post(rq,rqdata);

    if (!waitForReply(rpl)) return false;

    QByteArray ra = rpl->readAll();

    if (rpl->error()!=QNetworkReply::NoError) {
        qCritical() << "Bing auth error: " << rpl->error();
        qCritical() << "Response: " << ra;
        return false;
    }

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        qCritical() << "Bing auth: incorrect JSON auth response";
        qCritical() << "Response: " << ra;
        return false;
    }

    QJsonObject jroot = jdoc.object();
    QString token = jroot.value("access_token").toString();
    if (token.isEmpty()) {
        qCritical() << "Bing auth: empty JSON token";
        qCritical() << "Response: " << ra;
        return false;
    }

    authHeader = QString("Bearer %1").arg(token);
    srcLang = gSet->getSourceLanguageID(TE_BINGAPI);
    return true;
}

QString CBingTranslator::tranString(QString src)
{
    if (!isReady()) return QString();

    if (src.length()>=10000) {
        // Split by separator chars
        QRegExp rx("(\\ |\\,|\\.|\\:|\\t)"); //RegEx for ' ' or ',' or '.' or ':' or '\t'
        QStringList srcl = src.split(rx);
        // Check for max length
        QStringList srout;
        srout.clear();
        for (int i=0;i<srcl.count();i++) {
            QString s = srcl.at(i);
            while (!s.isEmpty()) {
                srout << s.left(9990);
                s.remove(0,9990);
            }
        }
        srcl.clear();
        QString res;
        res.clear();
        for (int i=0;i<srout.count();i++) {
            QString s = tranStringInternal(srout.at(i));
            if (s.startsWith("ERR:")) {
                res=s;
                break;
            }
            res+=s;
        }
        return res;
    } else
        return tranStringInternal(src);
}

QString CBingTranslator::tranStringInternal(QString src)
{
    QUrl rqurl = QUrl("http://api.microsofttranslator.com/v2/Http.svc/Translate");

    QUrlQuery rqData;
    rqData.addQueryItem("text",QUrl::toPercentEncoding(src));
    rqData.addQueryItem("from",srcLang);
    rqData.addQueryItem("to","en");
    rqurl.setQuery(rqData);

    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Authorization",authHeader.toUtf8());

    QNetworkReply *rpl = nam->get(rq);

    if (!waitForReply(rpl)) return QString("ERR:BING_NETWORK_ERROR");

    QByteArray ra = rpl->readAll();

    QDomDocument xdoc;
    if (!xdoc.setContent(ra))
        return QString("ERR:BING_XML_ERROR");

    QString res;
    res.clear();
    QDomNodeList xlist = xdoc.elementsByTagName("string");
    for (int i=0;i<xlist.count();i++) {
        QDomNode xnode = xlist.item(i);
        if (xnode.isElement()) {
            QDomElement xelem = xnode.toElement();
            res+=xelem.text();
        }
    }
    return res;
}

void CBingTranslator::doneTran(bool)
{
    authHeader.clear();
}

bool CBingTranslator::isReady()
{
    return !authHeader.isEmpty() && nam!=NULL;
}
