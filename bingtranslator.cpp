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
    initNAM();

    QUrlQuery uq;
    uq.addQueryItem(QStringLiteral("Subscription-Key"),clientKey);
    QUrl rqurl = QUrl(QStringLiteral("https://api.cognitive.microsoft.com/sts/v1.0/issueToken"));
    rqurl.setQuery(uq);
    QNetworkRequest rq(rqurl);

    QNetworkReply *rpl = nam()->post(rq,QByteArray());

    if (!waitForReply(rpl)) {
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

    authHeader = QStringLiteral("Bearer %1").arg(QString::fromUtf8(ra));
    clearErrorMsg();
    return true;
}

QString CBingTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl(QStringLiteral("https://api.cognitive.microsofttranslator.com/translate"));

    QUrlQuery rqData;
    rqData.addQueryItem(QStringLiteral("textType"),QStringLiteral("plain"));
    rqData.addQueryItem(QStringLiteral("from"),language().langFrom.bcp47Name());
    rqData.addQueryItem(QStringLiteral("to"),language().langTo.bcp47Name());
    rqData.addQueryItem(QStringLiteral("api-version"),QStringLiteral("3.0"));
    rqurl.setQuery(rqData);

    QJsonObject obj = QJsonObject();
    QJsonObject reqtext;
    reqtext[QStringLiteral("Text")] = src;
    QJsonArray reqlist({ reqtext });
    QJsonDocument doc(reqlist);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Authorization",authHeader.toUtf8());
    rq.setRawHeader("Content-Type","application/json");
    rq.setRawHeader("Content-Length",authHeader.toUtf8());

    QNetworkReply *rpl = nam()->post(rq,body);

    if (!waitForReply(rpl)) {
        setErrorMsg(QStringLiteral("ERROR: Bing translator network error"));
        return QStringLiteral("ERROR:TRAN_BING_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();

    rpl->deleteLater();

    doc = QJsonDocument::fromJson(ra);
    if (doc.isNull()) {
        setErrorMsg(QStringLiteral("ERROR: Bing translator JSON error"));
        return QStringLiteral("ERROR:TRAN_BING_JSON_ERROR");
    }

    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QJsonValue err = obj.value(QStringLiteral("error"));
        if (err.isObject()) {
            setErrorMsg(tr("ERROR: Bing translator JSON error #%1: %2")
                    .arg(err.toObject().value(QStringLiteral("code")).toInt())
                    .arg(err.toObject().value(QStringLiteral("message")).toString()));
            return QStringLiteral("ERROR:TRAN_BING_JSON_ERROR");
        }
        setErrorMsg(QStringLiteral("ERROR: Bing translator JSON generic error"));
        return QStringLiteral("ERROR:TRAN_BING_JSON_ERROR");
    }

    QString res;
    const QJsonArray rootlist = doc.array();
    for (const QJsonValue &rv : qAsConst(rootlist)) {
        if (!rv.isObject() ||
                !rv.toObject().contains(QStringLiteral("translations")) ||
                !rv.toObject().value(QStringLiteral("translations")).isArray()) continue;
        const QJsonArray translist = rv.toObject().value(QStringLiteral("translations")).toArray();
        for (const QJsonValue &tv : qAsConst(translist)) {
            if (tv.isObject() &&
                    tv.toObject().contains(QStringLiteral("text"))) {
                res+=tv.toObject().value(QStringLiteral("text")).toString();
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
