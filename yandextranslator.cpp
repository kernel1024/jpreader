#include <QUrl>

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>
#include <QVariant>
#include <QUrlQuery>

#include "globalcontrol.h"
#include "yandextranslator.h"

CYandexTranslator::CYandexTranslator(QObject *parent, const QString &SrcLang, const QString &yandexKey)
    : CWebAPIAbstractTranslator(parent, SrcLang)
{
    clientKey = yandexKey;
}

bool CYandexTranslator::initTran()
{
    if (nam==NULL)
        nam=new QNetworkAccessManager(this);

    if (gSet->settings.proxyUseTranslator)
        nam->setProxy(QNetworkProxy::DefaultProxy);
    else
        nam->setProxy(QNetworkProxy::NoProxy);

    tranError.clear();
    return true;
}

QString CYandexTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl("https://translate.yandex.net/api/v1.5/tr.json/translate");

    QUrlQuery rqData;
    rqData.addQueryItem("key",clientKey);
    rqData.addQueryItem("lang",QString("%1-en").arg(srcLang));
    rqData.addQueryItem("text",QUrl::toPercentEncoding(src));

    QNetworkRequest rq(rqurl);
    rq.setHeader(QNetworkRequest::ContentTypeHeader,
                 "application/x-www-form-urlencoded");

    QNetworkReply *rpl = nam->post(rq,rqData.toString(QUrl::FullyEncoded).toUtf8());

    if (!waitForReply(rpl)) {
        tranError = QString("ERROR: Yandex translator network error");
        return QString("ERROR:TRAN_YANDEX_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        tranError = QString("ERROR: Yandex translator JSON error");
        return QString("ERROR:TRAN_YANDEX_JSON_ERROR");
    }

    QJsonObject jroot = jdoc.object();
    int code = jroot.value("code").toInt(500);

    if (code!=200 || !jroot.value("text").isArray()) {
        qCritical() << "Yandex error:" << jroot.value("message").toString();
        tranError = QString("ERROR: Yandex translator error: %1").arg(jroot.value("message").toString());
        return QString("ERROR:TRAN_YANDEX_TRAN_ERROR");
    }

    QVariantList vl = jroot.value("text").toArray().toVariantList();
    QString res;
    res.clear();

    foreach (const QVariant& value, vl)
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

