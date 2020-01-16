#include <QUrl>

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QNetworkReply>

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
    initNAM();

    QUrlQuery uq;
    uq.addQueryItem(QSL("Subscription-Key"),clientKey);
    QUrl rqurl = QUrl(QSL("https://api.cognitive.microsoft.com/sts/v1.0/issueToken"));
    rqurl.setQuery(uq);
    QNetworkRequest rq(rqurl);

    QNetworkReply *rpl = nam()->post(rq,QByteArray());

    int status;
    if (!waitForReply(rpl,&status)) {
        setErrorMsg(tr("Bing connection error"));
        qCritical() << "Bing connection error: " << rpl->error();
        rpl->deleteLater();
        return false;
    }

    QByteArray ra = rpl->readAll();

    if (rpl->error()!=QNetworkReply::NoError) {
        setErrorMsg(tr("Bing auth error"));
        qCritical() << "Bing auth error: " << rpl->error();
        qCritical() << "Response: " << ra;
        rpl->deleteLater();
        return false;
    }

    rpl->deleteLater();

    authHeader = QSL("Bearer %1").arg(QString::fromUtf8(ra));
    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine CBingTranslator::engine()
{
    return CStructures::teBingAPI;
}

QString CBingTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl(QSL("https://api.cognitive.microsofttranslator.com/translate"));

    QUrlQuery rqData;
    rqData.addQueryItem(QSL("textType"),QSL("plain"));
    rqData.addQueryItem(QSL("from"),language().langFrom.bcp47Name());
    rqData.addQueryItem(QSL("to"),language().langTo.bcp47Name());
    rqData.addQueryItem(QSL("api-version"),QSL("3.0"));
    rqurl.setQuery(rqData);

    QJsonObject obj = QJsonObject();
    QJsonObject reqtext;
    reqtext[QSL("Text")] = src;
    QJsonArray reqlist({ reqtext });
    QJsonDocument doc(reqlist);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Authorization",authHeader.toUtf8());
    rq.setRawHeader("Content-Type","application/json");
    rq.setRawHeader("Content-Length",authHeader.toUtf8());

    QNetworkReply *rpl = nam()->post(rq,body);

    int status;
    if (!waitForReply(rpl,&status)) {
        setErrorMsg(QSL("ERROR: Bing translator network error"));
        return QSL("ERROR:TRAN_BING_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();

    rpl->close();
    rpl->deleteLater();

    doc = QJsonDocument::fromJson(ra);
    if (doc.isNull()) {
        setErrorMsg(QSL("ERROR: Bing translator JSON error"));
        return QSL("ERROR:TRAN_BING_JSON_ERROR");
    }

    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QJsonValue err = obj.value(QSL("error"));
        if (err.isObject()) {
            setErrorMsg(tr("ERROR: Bing translator JSON error #%1: %2")
                    .arg(err.toObject().value(QSL("code")).toInt())
                    .arg(err.toObject().value(QSL("message")).toString()));
            return QSL("ERROR:TRAN_BING_JSON_ERROR");
        }
        setErrorMsg(QSL("ERROR: Bing translator JSON generic error"));
        return QSL("ERROR:TRAN_BING_JSON_ERROR");
    }

    QString res;
    const QJsonArray rootlist = doc.array();
    for (const QJsonValue &rv : qAsConst(rootlist)) {
        const QJsonArray translist = rv.toObject().value(QSL("translations")).toArray();
        for (const QJsonValue &tv : qAsConst(translist)) {
            if (tv.toObject().contains(QSL("text"))) {
                res+=tv.toObject().value(QSL("text")).toString();
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
