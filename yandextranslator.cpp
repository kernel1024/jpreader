#include <QUrl>

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>
#include <QVariant>
#include <QUrlQuery>

#include "globalcontrol.h"
#include "genericfuncs.h"
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

QString CYandexTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl(QStringLiteral("https://translate.yandex.net/api/v1.5/tr.json/translate"));

    QUrlQuery rqData;
    rqData.addQueryItem(QStringLiteral("key"),clientKey);
    rqData.addQueryItem(QStringLiteral("lang"),QStringLiteral("%1-%2").arg(
                            language().langFrom.bcp47Name(),
                            language().langTo.bcp47Name()));
    rqData.addQueryItem(QStringLiteral("text"),QUrl::toPercentEncoding(src));

    QNetworkRequest rq(rqurl);
    rq.setHeader(QNetworkRequest::ContentTypeHeader,
                 "application/x-www-form-urlencoded");

    QNetworkReply *rpl = nam->post(rq,rqData.toString(QUrl::FullyEncoded).toUtf8());

    if (!waitForReply(rpl)) {
        setErrorMsg(tr("ERROR: Yandex translator network error"));
        rpl->deleteLater();
        return QStringLiteral("ERROR:TRAN_YANDEX_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();

    rpl->deleteLater();

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        setErrorMsg(tr("ERROR: Yandex translator JSON error"));
        return QStringLiteral("ERROR:TRAN_YANDEX_JSON_ERROR");
    }

    QJsonObject jroot = jdoc.object();
    int code = jroot.value(QStringLiteral("code")).toInt(httpCodeServerError);

    if (code!=httpCodeFound || !jroot.value(QStringLiteral("text")).isArray()) {
        qCritical() << "Yandex error:" << jroot.value(QStringLiteral("message")).toString();
        setErrorMsg(QStringLiteral("ERROR: Yandex translator error: %1")
                    .arg(jroot.value(QStringLiteral("message")).toString()));
        return QStringLiteral("ERROR:TRAN_YANDEX_TRAN_ERROR");
    }

    QString res;
    res.clear();

    const QVariantList vl = jroot.value(QStringLiteral("text")).toArray().toVariantList();
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

