#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "yandexcloudtranslator.h"

CYandexCloudTranslator::CYandexCloudTranslator(QObject *parent, const CLangPair &lang, const QString &apiKey,
                                               const QString &folderID)
    : CWebAPIAbstractTranslator(parent,lang)
{
    m_apiKey = apiKey;
    m_folderID = folderID;
}

bool CYandexCloudTranslator::initTran()
{
    initNAM();

    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine CYandexCloudTranslator::engine()
{
    return CStructures::teYandexCloud;
}

QString CYandexCloudTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl(QSL("https://translate.api.cloud.yandex.net/translate/v2/translate"));

    QString apiKey = QSL("Api-Key %1").arg(m_apiKey);
    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Content-Type","application/json");
    rq.setRawHeader("Authorization",apiKey.toLatin1());

    QJsonObject reqtext;
    reqtext[QSL("sourceLanguageCode")] = language().langFrom.bcp47Name();
    reqtext[QSL("targetLanguageCode")] = language().langTo.bcp47Name();
    reqtext[QSL("format")] = QSL("PLAIN_TEXT");
    reqtext[QSL("folderId")] = m_folderID;
    QJsonArray texts({ src });
    reqtext[QSL("texts")] = texts;
    QJsonDocument doc(reqtext);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    QNetworkReply *rpl = nam()->post(rq,body);

    int status;
    if (!waitForReply(rpl,&status)) {
        setErrorMsg(tr("ERROR: Yandex Cloud translator network error"));
        rpl->deleteLater();
        return QSL("ERROR:TRAN_YANDEX_CLOUD_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();
    rpl->close();
    rpl->deleteLater();

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        setErrorMsg(tr("ERROR: Yandex Cloud translator JSON error"));
        return QSL("ERROR:TRAN_YANDEX_CLOUD_JSON_ERROR");
    }

    QJsonObject jroot = jdoc.object();
    QJsonValue err = jroot.value(QSL("message"));
    QJsonValue errCode = jroot.value(QSL("code"));
    if (err.isString()) {
        setErrorMsg(tr("ERROR: Yandex Cloud translator JSON error #%1: %2, HTTP status: %3")
                    .arg(errCode.toString(),err.toString())
                    .arg(status));
        return QSL("ERROR:TRAN_YANDEX_CLOUD_JSON_ERROR");
    }

    if (status!=CDefaults::httpCodeFound) {
        setErrorMsg(QSL("ERROR: Yandex Cloud translator HTTP generic error, HTTP status: %1").arg(status));
        return QSL("ERROR:TRAN_YANDEX_CLOUD_HTTP_ERROR");
    }

    const QJsonArray translations = jroot.value(QSL("translations")).toArray();
    if (translations.isEmpty()) {
        setErrorMsg(QSL("ERROR: Yandex Cloud translator 'translations' member missing from JSON response, "
                        "HTTP status: %1").arg(status));
        return QSL("ERROR:TRAN_YANDEX_CLOUD_RESPONSE_ERROR");
    }

    QString res;
    for (const auto& t : translations) {
        QJsonValue text = t.toObject().value(QSL("text"));
        if (text.isString())
            res.append(text.toString());
    }
    return res;
}

void CYandexCloudTranslator::clearCredentials()
{

}

bool CYandexCloudTranslator::isValidCredentials()
{
    return (!m_apiKey.isEmpty() && !m_folderID.isEmpty());
}
