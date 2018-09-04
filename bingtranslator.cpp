#include <QUrl>

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>
#include <QUrlQuery>

#include "bingtranslator.h"
#include "globalcontrol.h"

CBingTranslator::CBingTranslator(QObject *parent, const CLangPair &lang,
                                 const QString &bingKey)
    : CWebAPIAbstractTranslator(parent, lang)
{
    clientKey = bingKey;
    clearCredentials();
}

bool CBingTranslator::initTran()
{
    if (nam==nullptr)
        nam=new QNetworkAccessManager(this);

    if (gSet->settings.proxyUseTranslator)
        nam->setProxy(QNetworkProxy::DefaultProxy);
    else
        nam->setProxy(QNetworkProxy::NoProxy);

    QUrlQuery uq;
    uq.addQueryItem("Subscription-Key",clientKey);
    QUrl rqurl = QUrl("https://api.cognitive.microsoft.com/sts/v1.0/issueToken");
    rqurl.setQuery(uq);
    QNetworkRequest rq(rqurl);

    QNetworkReply *rpl = nam->post(rq,QByteArray());

    if (!waitForReply(rpl)) {
        tranError = tr("Bing connection error");
        qCritical() << "Bing connection error: " << rpl->error();
        rpl->deleteLater();
        return false;
    }

    QByteArray ra = rpl->readAll();

    if (rpl->error()!=QNetworkReply::NoError) {
        tranError = tr("Bing auth error");
        qCritical() << "Bing auth error: " << rpl->error();
        qCritical() << "Response: " << ra;
        rpl->deleteLater();
        return false;
    }

    rpl->deleteLater();

    authHeader = QString("Bearer %1").arg(QString::fromUtf8(ra));
    tranError.clear();
    return true;
}

QString CBingTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl("https://api.cognitive.microsofttranslator.com/translate");

    QUrlQuery rqData;
    rqData.addQueryItem("textType","plain");
    rqData.addQueryItem("from",m_lang.langFrom.bcp47Name());
    rqData.addQueryItem("to",m_lang.langTo.bcp47Name());
    rqData.addQueryItem("api-version","3.0");
    rqurl.setQuery(rqData);

    QJsonObject obj = QJsonObject();
    QJsonObject reqtext;
    reqtext["Text"] = src;
    QJsonArray reqlist({ reqtext });
    QJsonDocument doc(reqlist);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Authorization",authHeader.toUtf8());
    rq.setRawHeader("Content-Type","application/json");
    rq.setRawHeader("Content-Length",authHeader.toUtf8());

    QNetworkReply *rpl = nam->post(rq,body);

    if (!waitForReply(rpl)) {
        tranError = QString("ERROR: Bing translator network error");
        return QString("ERROR:TRAN_BING_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();

    rpl->deleteLater();

    doc = QJsonDocument::fromJson(ra);
    if (doc.isNull()) {
        tranError = QString("ERROR: Bing translator JSON error");
        return QString("ERROR:TRAN_BING_JSON_ERROR");
    }

    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QJsonValue err = obj.value("error");
        if (err.isObject()) {
            tranError = QString("ERROR: Bing translator JSON error #%1: %2")
                    .arg(err.toObject().value("code").toInt())
                    .arg(err.toObject().value("message").toString());
            return QString("ERROR:TRAN_BING_JSON_ERROR");
        }
        tranError = QString("ERROR: Bing translator JSON generic error");
        return QString("ERROR:TRAN_BING_JSON_ERROR");
    }

    QString res;
    QJsonArray rootlist = doc.array();
    foreach (const QJsonValue &rv, rootlist) {
        if (!rv.isObject() ||
                !rv.toObject().contains("translations") ||
                !rv.toObject().value("translations").isArray()) continue;
        QJsonArray translist = rv.toObject().value("translations").toArray();
        foreach (const QJsonValue &tv, translist) {
            if (tv.isObject() &&
                    tv.toObject().contains("text")) {
                res+=tv.toObject().value("text").toString();
            }

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
