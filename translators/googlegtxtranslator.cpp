#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>
#include <QUrlQuery>
#include <QDebug>

#include "googlegtxtranslator.h"

CGoogleGTXTranslator::CGoogleGTXTranslator(QObject *parent, const CLangPair& lang)
    : CWebAPIAbstractTranslator(parent, lang)
{
}

bool CGoogleGTXTranslator::initTran()
{
    initNAM();

    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine CGoogleGTXTranslator::engine()
{
    return CStructures::teGoogleGTX;
}

QString CGoogleGTXTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl(QString::fromUtf8(QByteArray::fromBase64(
        "aHR0cHM6Ly90cmFuc2xhdGUuZ29vZ2xlYXBpcy5jb20vdHJhbnNsYXRlX2Evc2luZ2xl")));

    QUrlQuery rqData;
    rqData.addQueryItem(QSL("client"),QSL("gtx"));
    rqData.addQueryItem(QSL("sl"),language().langFrom.bcp47Name());
    rqData.addQueryItem(QSL("tl"),language().langTo.bcp47Name());
    rqData.addQueryItem(QSL("dt"),QSL("t"));
    rqData.addQueryItem(QSL("q"),QString::fromUtf8(QUrl::toPercentEncoding(src)));

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
        return QSL("ERROR:TRAN_GOOGLE_GTX_ABORTED");
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(QSL("ERROR: Yandex translator network error"));
        return QSL("ERROR:TRAN_GOOGLE_GTX_NETWORK_ERROR");
    }

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty() || !jdoc.isArray()) {
        setErrorMsg(QSL("ERROR: Google GTX translator JSON error"));
        qWarning() << ra;
        return QSL("ERROR:TRAN_GOOGLE_GTX_JSON_ERROR");
    }

    QString res;
    res.clear();
    bool okres = false;

    QJsonArray ja1 = jdoc.array();
    if (ja1.count()>0 && ja1.first().isArray()) {
        QJsonArray ja2 = ja1.first().toArray();
        if (ja2.count()>0 && ja2.first().isArray()) {
            QJsonArray ja3 = ja2.first().toArray();
            if (ja3.count()>0 && ja3.first().isString()) {
                res = ja3.first().toString();
                okres = true;
            }
        }
    }

    if (!okres) {
        qCritical() << "Google GTX error: empty result";
        setErrorMsg(QSL("ERROR: Google GTX translator error: empty result"));
        return QSL("ERROR:TRAN_GOOGLE_GTX_TRAN_ERROR");
    }

    return res;
}

bool CGoogleGTXTranslator::isValidCredentials()
{
    return true;
}
