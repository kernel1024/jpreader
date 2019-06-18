#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegExp>
#include <QVariant>
#include <QUrlQuery>
#include <QNetworkReply>

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

QString CGoogleGTXTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl(QString::fromUtf8(QByteArray::fromBase64(
        "aHR0cHM6Ly90cmFuc2xhdGUuZ29vZ2xlYXBpcy5jb20vdHJhbnNsYXRlX2Evc2luZ2xl")));

    QUrlQuery rqData;
    rqData.addQueryItem(QStringLiteral("client"),QStringLiteral("gtx"));
    rqData.addQueryItem(QStringLiteral("sl"),language().langFrom.bcp47Name());
    rqData.addQueryItem(QStringLiteral("tl"),language().langTo.bcp47Name());
    rqData.addQueryItem(QStringLiteral("dt"),QStringLiteral("t"));
    rqData.addQueryItem(QStringLiteral("q"),QString::fromUtf8(QUrl::toPercentEncoding(src)));

    QNetworkRequest rq(rqurl);
    rq.setHeader(QNetworkRequest::ContentTypeHeader,
                 "application/x-www-form-urlencoded");

    QNetworkReply *rpl = nam()->post(rq,rqData.toString(QUrl::FullyEncoded).toUtf8());

    if (!waitForReply(rpl)) {
        setErrorMsg(QStringLiteral("ERROR: Google GTX translator network error"));
        qWarning() << rpl->errorString();
        rpl->deleteLater();
        return QStringLiteral("ERROR:TRAN_GOOGLE_GTX_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();

    rpl->close();
    rpl->deleteLater();

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty() || !jdoc.isArray()) {
        setErrorMsg(QStringLiteral("ERROR: Google GTX translator JSON error"));
        qWarning() << ra;
        return QStringLiteral("ERROR:TRAN_GOOGLE_GTX_JSON_ERROR");
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
        setErrorMsg(QStringLiteral("ERROR: Google GTX translator error: empty result"));
        return QStringLiteral("ERROR:TRAN_GOOGLE_GTX_TRAN_ERROR");
    }

    return res;
}

void CGoogleGTXTranslator::clearCredentials()
{
}

bool CGoogleGTXTranslator::isValidCredentials()
{
    return true;
}
