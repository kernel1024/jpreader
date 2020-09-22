#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QDebug>

#include "../globalcontrol.h"
#include "../genericfuncs.h"
#include "yandextranslator.h"

CYandexTranslator::CYandexTranslator(QObject *parent, const CLangPair &lang, const QString &yandexKey)
    : CWebAPIAbstractTranslator(parent, lang)
{
    clientKey = yandexKey;
}

bool CYandexTranslator::initTran()
{
    initNAM();

    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine CYandexTranslator::engine()
{
    return CStructures::teYandexAPI;
}

QString CYandexTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl(QSL("https://translate.yandex.net/api/v1.5/tr.json/translate"));

    QUrlQuery rqData;
    rqData.addQueryItem(QSL("key"),clientKey);
    rqData.addQueryItem(QSL("lang"),QSL("%1-%2").arg(
                            language().langFrom.bcp47Name(),
                            language().langTo.bcp47Name()));
    rqData.addQueryItem(QSL("text"),QString::fromLatin1(QUrl::toPercentEncoding(src)));

    QNetworkRequest rq(rqurl);
    rq.setHeader(QNetworkRequest::ContentTypeHeader,
                 "application/x-www-form-urlencoded");

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [rq]() -> QNetworkRequest {
        return rq;
    };
    QByteArray ra = processRequest(requestMaker,rqData.toString(QUrl::FullyEncoded).toUtf8(),
                                   &status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: Yandex translator aborted by user request"));
        return QSL("ERROR:TRAN_YANDEX_ABORTED");
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(QSL("ERROR: Yandex translator network error"));
        return QSL("ERROR:TRAN_YANDEX_NETWORK_ERROR");
    }

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        setErrorMsg(tr("ERROR: Yandex translator JSON error"));
        return QSL("ERROR:TRAN_YANDEX_JSON_ERROR");
    }

    QJsonObject jroot = jdoc.object();
    int code = jroot.value(QSL("code")).toInt(CDefaults::httpCodeServerError);

    if (code!=CDefaults::httpCodeFound || !jroot.value(QSL("text")).isArray()) {
        qCritical() << "Yandex error:" << jroot.value(QSL("message")).toString();
        setErrorMsg(QSL("ERROR: Yandex translator error: %1")
                    .arg(jroot.value(QSL("message")).toString()));
        return QSL("ERROR:TRAN_YANDEX_TRAN_ERROR");
    }

    QString res;
    res.clear();

    const QVariantList vl = jroot.value(QSL("text")).toArray().toVariantList();
    for (const QVariant& value : vl)
        res += value.toString();

    return res;
}

void CYandexTranslator::clearCredentials()
{
}

bool CYandexTranslator::isValidCredentials()
{
    return !clientKey.isEmpty();
}

