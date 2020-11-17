#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "bingtranslator.h"
#include "global/globalcontrol.h"
#include "utils/genericfuncs.h"

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

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [rq]() -> QNetworkRequest {
        return rq;
    };
    QByteArray ra = processRequest(requestMaker,QByteArray(),&status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: Bing translator aborted by user request"));
        return false;
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(QSL("ERROR: Bing translator auth error"));
        return false;
    }

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

    QJsonObject reqtext;
    reqtext[QSL("Text")] = src;
    QJsonArray reqlist({ reqtext });
    QJsonDocument doc(reqlist);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Authorization",authHeader.toUtf8());
    rq.setRawHeader("Content-Type","application/json");
    rq.setRawHeader("Content-Length",authHeader.toUtf8());

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [rq]() -> QNetworkRequest {
        return rq;
    };
    QByteArray ra = processRequest(requestMaker,body,&status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: Bing translator aborted by user request"));
        return QSL("ERROR:TRAN_BING_ABORTED");
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(QSL("ERROR: Bing translator network error"));
        return QSL("ERROR:TRAN_BING_NETWORK_ERROR");
    }

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
