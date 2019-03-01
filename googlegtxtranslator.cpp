#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegExp>
#include <QVariant>
#include <QUrlQuery>

#include "googlegtxtranslator.h"

CGoogleGTXTranslator::CGoogleGTXTranslator(QObject *parent, const CLangPair& lang)
    : CWebAPIAbstractTranslator(parent, lang)
{
}

bool CGoogleGTXTranslator::initTran()
{
    if (nam==nullptr) {
        nam=new QNetworkAccessManager(this);
        auto cj = new CNetworkCookieJar();
        nam->setCookieJar(cj);
    }

    auto cj = qobject_cast<CNetworkCookieJar *>(nam->cookieJar());
    auto mj = qobject_cast<CNetworkCookieJar *>(gSet->auxNetManager->cookieJar());
    cj->initAllCookies(mj->getAllCookies());

    if (gSet->settings.proxyUseTranslator)
        nam->setProxy(QNetworkProxy::DefaultProxy);
    else
        nam->setProxy(QNetworkProxy::NoProxy);

    tranError.clear();
    return true;
}

QString CGoogleGTXTranslator::tranStringInternal(const QString &src)
{
    QUrl rqurl = QUrl(QString::fromUtf8(QByteArray::fromBase64(
        "aHR0cHM6Ly90cmFuc2xhdGUuZ29vZ2xlYXBpcy5jb20vdHJhbnNsYXRlX2Evc2luZ2xl")));

    QUrlQuery rqData;
    rqData.addQueryItem(QStringLiteral("client"),QStringLiteral("gtx"));
    rqData.addQueryItem(QStringLiteral("sl"),m_lang.langFrom.bcp47Name());
    rqData.addQueryItem(QStringLiteral("tl"),m_lang.langTo.bcp47Name());
    rqData.addQueryItem(QStringLiteral("dt"),QStringLiteral("t"));
    rqData.addQueryItem(QStringLiteral("q"),QUrl::toPercentEncoding(src));

    QNetworkRequest rq(rqurl);
    rq.setHeader(QNetworkRequest::ContentTypeHeader,
                 "application/x-www-form-urlencoded");

    QNetworkReply *rpl = nam->post(rq,rqData.toString(QUrl::FullyEncoded).toUtf8());

    if (!waitForReply(rpl)) {
        tranError = QStringLiteral("ERROR: Google GTX translator network error");
        qWarning() << rpl->errorString();
        rpl->deleteLater();
        return QStringLiteral("ERROR:TRAN_GOOGLE_GTX_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();

    rpl->deleteLater();

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty() || !jdoc.isArray()) {
        tranError = QStringLiteral("ERROR: Google GTX translator JSON error");
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
        tranError = QStringLiteral("ERROR: Google GTX translator error: empty result");
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
