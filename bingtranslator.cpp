#include <QUrl>

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

CBingTranslator::CBingTranslator(QObject *parent, const QString &SrcLang, const QString &bingID,
                                 const QString &bingKey)
    : CWebAPIAbstractTranslator(parent, SrcLang)
{
    clientID = bingID;
    clientKey = bingKey;
    clearCredentials();
}

bool CBingTranslator::initTran()
{
    if (nam==NULL)
        nam=new QNetworkAccessManager(this);

    if (gSet->settings.proxyUseTranslator)
        nam->setProxy(QNetworkProxy::DefaultProxy);
    else
        nam->setProxy(QNetworkProxy::NoProxy);

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
        tranError = tr("Bing auth error");
        qCritical() << "Bing auth error: " << rpl->error();
        qCritical() << "Response: " << ra;
        return false;
    }

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        tranError = tr("Bing auth error: incorrect JSON auth response");
        qCritical() << "Bing auth: incorrect JSON auth response";
        qCritical() << "Response: " << ra;
        return false;
    }

    QJsonObject jroot = jdoc.object();
    QString token = jroot.value("access_token").toString();
    if (token.isEmpty()) {
        tranError = tr("Bing auth error: empty JSON token");
        qCritical() << "Bing auth: empty JSON token";
        qCritical() << "Response: " << ra;
        return false;
    }

    authHeader = QString("Bearer %1").arg(token);
    tranError.clear();
    return true;
}

QString CBingTranslator::tranStringInternal(const QString &src)
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

    if (!waitForReply(rpl)) {
        tranError = QString("ERROR: Bing translator network error");
        return QString("ERROR:TRAN_BING_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();

    QDomDocument xdoc;
    if (!xdoc.setContent(ra)) {
        tranError = QString("ERROR: Bing translator XML error");
        return QString("ERROR:TRAN_BING_XML_ERROR");
    }

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

void CBingTranslator::clearCredentials()
{
    authHeader.clear();
}

bool CBingTranslator::isValidCredentials()
{
    return !authHeader.isEmpty();
}
