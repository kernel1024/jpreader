#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "deeplapitranslator.h"
#include "utils/genericfuncs.h"

CDeeplAPITranslator::CDeeplAPITranslator(QObject *parent, const CLangPair &lang, CStructures::DeeplAPIMode apiMode,
                                         const QString &apiKey, CStructures::DeeplAPISplitSentences splitSentences,
                                         CStructures::DeeplAPIFormality formality)
    : CWebAPIAbstractTranslator(parent,lang),
      m_apiKey(apiKey),
      m_apiMode(apiMode),
      m_splitSentences(splitSentences),
      m_formality(formality)
{
}

QUrl CDeeplAPITranslator::queryUrl(const QString &method) const
{
    QString host = QSL("api.deepl.com");
    if (m_apiMode == CStructures::deeplAPIModeFree)
        host = QSL("api-free.deepl.com");

    return QUrl(QSL("https://%1/v2/%2").arg(host,method));
}

QString CDeeplAPITranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = queryUrl(QSL("translate"));

    QUrlQuery rqData;
    rqData.addQueryItem(QSL("source_lang"),language().langFrom.bcp47Name());
    rqData.addQueryItem(QSL("target_lang"),language().langTo.bcp47Name());
    rqData.addQueryItem(QSL("text"),QString::fromUtf8(QUrl::toPercentEncoding(src)));
    if (m_splitSentences == CStructures::deeplAPISplitFull)
        rqData.addQueryItem(QSL("split_sentences"),QSL("1"));
    if (m_splitSentences == CStructures::deeplAPISplitNone)
        rqData.addQueryItem(QSL("split_sentences"),QSL("0"));
    if (m_splitSentences == CStructures::deeplAPISplitNoNewlines)
        rqData.addQueryItem(QSL("split_sentences"),QSL("nonewlines"));
    if (m_formality == CStructures::deeplAPIFormalityLess)
        rqData.addQueryItem(QSL("formality"),QSL("less"));
    if (m_formality == CStructures::deeplAPIFormalityMore)
        rqData.addQueryItem(QSL("formality"),QSL("more"));

    const QString apiKey = QSL("DeepL-Auth-Key %1").arg(m_apiKey);
    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Authorization",apiKey.toLatin1());
    rq.setRawHeader("Accept","application/json");
    rq.setRawHeader("Content-Type","application/x-www-form-urlencoded");

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [rq]() -> QNetworkRequest {
        return rq;
    };
    QByteArray ra = processRequest(requestMaker,rqData.toString(QUrl::FullyEncoded).toUtf8(),
                                   &status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: DeepL API translator aborted by user request"));
        return QSL("ERROR:TRAN_DEEPL_API_ABORTED");
    }
    if (status == CDefaults::httpQuotaExceeded) {
        setErrorMsg(tr("ERROR: DeepL API quota exceeded"));
        return QSL("ERROR:TRAN_DEEPL_API_QUOTA_EXCEEDED");
    }
    if (ra.isEmpty()) {
        setErrorMsg(tr("ERROR: DeepL API translator empty response"));
        return QSL("ERROR:TRAN_DEEPL_API_NETWORK_ERROR");
    }

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        setErrorMsg(tr("ERROR: DeepL API translator JSON error"));
        return QSL("ERROR:TRAN_DEEPL_API_JSON_ERROR");
    }

    QJsonObject jroot = jdoc.object();
    QJsonValue err = jroot.value(QSL("message"));
    if (err.isString()) {
        setErrorMsg(tr("ERROR: DeepL API translator internal error.\nMessage: %1,\nHTTP status: %2")
                    .arg(err.toString())
                    .arg(status));
        return QSL("ERROR:TRAN_DEEPL_API_GENERIC_ERROR");
    }

    if (status != CDefaults::httpCodeFound) {
        setErrorMsg(QSL("ERROR: DeepL API translator HTTP generic error.\nHTTP status: %1").arg(status));
        return QSL("ERROR:TRAN_DEEPL_API_HTTP_ERROR");
    }

    const QJsonArray translations = jroot.value(QSL("translations")).toArray();
    if (translations.isEmpty()) {
        setErrorMsg(QSL("ERROR: DeepL API translator result member missing from JSON response, "
                        "HTTP status: %1").arg(status));
        return QSL("ERROR:TRAN_DEEPL_API_RESPONSE_ERROR");
    }

    QString res;
    for (const auto &tran : translations) {
        if (!res.isEmpty())
            res.append(QSL(" "));
        res.append(tran.toObject().value(QSL("text")).toString());
    }

    return res;
}

bool CDeeplAPITranslator::isValidCredentials()
{
    return (!m_apiKey.isEmpty() && queryUrl(QSL("TranslateText")).isValid());
}

bool CDeeplAPITranslator::initTran()
{
    initNAM();

    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine CDeeplAPITranslator::engine()
{
    return CStructures::teDeeplAPI;
}
