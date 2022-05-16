#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include "promtnmttranslator.h"
#include "utils/genericfuncs.h"

CPromtNmtTranslator::CPromtNmtTranslator(QObject *parent, const CLangPair &lang, const QString &serverUrl,
                                         const QString &apiKey)
    : CWebAPIAbstractTranslator(parent,lang), m_apiKey(apiKey), m_serverUrl(serverUrl)
{
}

bool CPromtNmtTranslator::initTran()
{
    initNAM();

    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine CPromtNmtTranslator::engine()
{
    return CStructures::tePromtNmtAPI;
}

QUrl CPromtNmtTranslator::queryUrl(const QString &method) const
{
    return QUrl(QSL("%1/Services/v1/rest.svc/%2").arg(m_serverUrl,method));
}

QString CPromtNmtTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = queryUrl(QSL("TranslateText"));

    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Content-Type","application/json");
    rq.setRawHeader("PTSAPIKEY",m_apiKey.toLatin1());
    rq.setRawHeader("Accept","application/json");

    QJsonObject reqtext;
    reqtext[QSL("text")] = src;
    reqtext[QSL("from")] = language().langFrom.bcp47Name();
    reqtext[QSL("to")] = language().langTo.bcp47Name();
    QJsonDocument doc(reqtext);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [rq]() -> QNetworkRequest {
        return rq;
    };
    QByteArray ra = processRequest(requestMaker,body,&status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: Promt NMT translator aborted by user request"));
        return QSL("ERROR:TRAN_PROMT_NMT_ABORTED");
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(tr("ERROR: Promt NMT translator network error"));
        return QSL("ERROR:TRAN_PROMT_NMT_NETWORK_ERROR");
    }

    if (ra.startsWith('"')) {
        ra.prepend(QSL("{ \"text\" : ").toUtf8());
        ra.append(QSL(" }").toUtf8());
    }

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        setErrorMsg(tr("ERROR: Promt NMT translator JSON error"));
        return QSL("ERROR:TRAN_PROMT_NMT_JSON_ERROR");
    }

    QJsonObject jroot = jdoc.object();
    QJsonValue err = jroot.value(QSL("message"));
    if (err.isString()) {
        setErrorMsg(tr("ERROR: Promt NMT translator JSON error #%1, HTTP status: %2")
                    .arg(err.toString())
                    .arg(status));
        return QSL("ERROR:TRAN_PROMT_NMT_JSON_ERROR");
    }

    if (status!=CDefaults::httpCodeFound) {
        setErrorMsg(QSL("ERROR: Promt NMT translator HTTP generic error, HTTP status: %1").arg(status));
        return QSL("ERROR:TRAN_PROMT_NMT_HTTP_ERROR");
    }

    QString res = jroot.value(QSL("text")).toString();
    if (res.isEmpty()) {
        setErrorMsg(QSL("ERROR: Promt NMT translator result member missing from JSON response, "
                        "HTTP status: %1").arg(status));
        return QSL("ERROR:TRAN_PROMT_NMT_RESPONSE_ERROR");
    }

    return res;
}

bool CPromtNmtTranslator::isValidCredentials()
{
    return (!m_apiKey.isEmpty() && !m_serverUrl.isEmpty() && queryUrl(QSL("TranslateText")).isValid());
}
