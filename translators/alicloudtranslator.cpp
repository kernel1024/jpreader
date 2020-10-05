#include <QDateTime>
#include <QCryptographicHash>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <algorithm>
#include "alicloudtranslator.h"

CAliCloudTranslator::CAliCloudTranslator(QObject *parent, const CLangPair &lang,
                                         CStructures::AliCloudTranslatorMode mode,
                                         const QString &accessKeyID,
                                         const QString &accessKeySecret)
    : CWebAPIAbstractTranslator(parent,lang)
{
    m_mode = mode;
    m_accessKeyID = accessKeyID;
    m_accessKeySecret = accessKeySecret;

    clearCredentials();
}

CStructures::TranslationEngine CAliCloudTranslator::engine()
{
    return CStructures::teAliCloud;
}

bool CAliCloudTranslator::isValidCredentials()
{
    return (!m_accessKeyID.isEmpty() && !m_accessKeySecret.isEmpty());
}

bool CAliCloudTranslator::initTran()
{
    if (!isValidCredentials())
        return false;

    const QStringList validSecondLangs({ QSL("en"), QSL("ko"), QSL("ja") });
    bool ecomValidPair = ((language().langFrom.bcp47Name().startsWith(QSL("zh"))
                           && validSecondLangs.contains(language().langTo.bcp47Name())) ||
                          (language().langTo.bcp47Name().startsWith(QSL("zh"))
                           && validSecondLangs.contains(language().langFrom.bcp47Name())));

    if (!language().isValid()) {
        setErrorMsg(QSL("Alibaba Cloud Translator: Unacceptable language pair. "
                        "Chinese <-> (english/korean/japanese) is supported."));
        qCritical() << "Alibaba Cloud Translator: Unacceptable language pair";
        return false;
    }

    if ((m_mode!=CStructures::aliTranslatorGeneral) && !ecomValidPair) {
        setErrorMsg(QSL("Alibaba Cloud Translator: Unacceptable language pair for E-Commerce mode. "
                        "Chinese <-> (english/korean/japanese) is supported."));
        qCritical() << "Alibaba Cloud Translator: Unacceptable language pair for E-Commerce mode";
        return false;
    }

    initNAM();

    clearErrorMsg();
    return true;
}

QNetworkRequest CAliCloudTranslator::createAliRequest(const CStringHash &body) const
{
    QUrl realUrl = QUrl(QSL("https://mt.cn-hangzhou.aliyuncs.com/"));
    const QString host = realUrl.host();
    QString action;
    if (m_mode == CStructures::aliTranslatorGeneral) {
        action = QSL("TranslateGeneral");
    } else {
        action = QSL("TranslateECommerce");
    }

    // HTTP headers
    const QDateTime dt = QDateTime::currentDateTimeUtc();
    const QString method = QSL("POST");
    const QString signatureMethod = QSL("HMAC-SHA1");
    const QString regionId = QSL("cn-hangzhou");

    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    CStringHash signParams;
    signParams[QSL("Action")] = action;
    signParams[QSL("Format")] = QSL("JSON");
    signParams[QSL("AccessKeyId")] = m_accessKeyID;
    signParams[QSL("Version")] = QSL("2018-10-12");
    signParams[QSL("SignatureVersion")] = QSL("1.0");
    signParams[QSL("SignatureNonce")] = uuid;
    signParams[QSL("SignatureMethod")] = signatureMethod;
    signParams[QSL("RegionId")] = regionId;
    signParams[QSL("SecurityToken")] = QString();
    signParams[QSL("Timestamp")] = dt.toString(Qt::ISODate);

    CStringHash queryParams = signParams;
    signParams.insert(body);

    QString stringToSign = QSL("%1&%2&%3").arg(method,
                                               QString::fromLatin1(QUrl::toPercentEncoding(realUrl.path())),
                                               QString::fromLatin1(QUrl::toPercentEncoding(canonicalizedQuery(signParams))));
    queryParams[QSL("Signature")] = QString::fromLatin1(CGenericFuncs::hmacSha1(
                                                            QSL("%1&").arg(m_accessKeySecret).toLatin1(),
                                                            stringToSign.toLatin1()).toBase64());

    QUrlQuery uq;
    for (auto it = queryParams.constKeyValueBegin(), end = queryParams.constKeyValueEnd(); it!=end; it++)
        uq.addQueryItem(it->first,it->second);
    realUrl.setQuery(uq);

    QNetworkRequest rq(realUrl);
    rq.setRawHeader("Content-Type","application/x-www-form-urlencoded");
    rq.setRawHeader("x-acs-action", action.toLatin1());
    rq.setRawHeader("x-sdk-client", "CPP/1.36.632");
    rq.setRawHeader("x-acs-version", "2018-10-12");
    rq.setRawHeader("Host", host.toLatin1());

    return rq;
}

QString CAliCloudTranslator::canonicalizedQuery(const CStringHash &params) const
{
    QList<QString> keys = params.keys();
    std::sort(keys.begin(),keys.end());

    QString res;
    for (const auto &key : qAsConst(keys)) {
        QString k = QString::fromLatin1(QUrl::toPercentEncoding(key));
        k.replace(QSL("+"), QSL("%20"));
        k.replace(QSL("*"), QSL("%2A"));
        k.replace(QSL("%7E"), QSL("~"));
        QString v = QString::fromLatin1(QUrl::toPercentEncoding(params.value(key)));
        v.replace(QSL("+"), QSL("%20"));
        v.replace(QSL("*"), QSL("%2A"));
        v.replace(QSL("%7E"), QSL("~"));

        if (!res.isEmpty())
            res.append('&');
        res.append(QSL("%1=%2").arg(k,v));
    }

    return res;
}

QString CAliCloudTranslator::tranStringInternal(const QString &src)
{
    CStringHash body;
    body[QSL("SourceLanguage")] = language().langFrom.bcp47Name();
    body[QSL("TargetLanguage")] = language().langTo.bcp47Name();
    body[QSL("FormatType")] = QSL("text");
    body[QSL("SourceText")] = src;
    switch (m_mode) {
        case CStructures::aliTranslatorECTitle: body[QSL("Scene")] = QSL("title"); break;
        case CStructures::aliTranslatorECDescription: body[QSL("Scene")] = QSL("description"); break;
        case CStructures::aliTranslatorECCommunication: body[QSL("Scene")] = QSL("communication"); break;
        case CStructures::aliTranslatorECMedical: body[QSL("Scene")] = QSL("medical"); break;
        case CStructures::aliTranslatorECSocial: body[QSL("Scene")] = QSL("social"); break;
        default: break;
    }

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [this,body]() -> QNetworkRequest {
        return createAliRequest(body);
    };
    QByteArray bodyData;
    for (auto it = body.constKeyValueBegin(), end = body.constKeyValueEnd(); it != end; it++) {
        if (!bodyData.isEmpty())
            bodyData.append('&');
        bodyData.append(QSL("%1=%2").arg(it->first,
                                         QString::fromLatin1(QUrl::toPercentEncoding(it->second)))
                        .toLatin1());
    }
    QByteArray ra = processRequest(requestMaker,bodyData,&status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: Alibaba Cloud Translation aborted by user request"));
        return QSL("ERROR:TRAN_ALI_ABORTED");
    }

    QJsonDocument doc;
    if (!ra.isEmpty())
        doc = QJsonDocument::fromJson(ra);

    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        QString serverMessage;
        if (!doc.isEmpty()) {
            serverMessage = QSL("\nCode: %1\nMessage:%2")
                            .arg(doc.object().value(QSL("Code")).toString(),
                                 doc.object().value(QSL("Message")).toString());
        }
        setErrorMsg(QSL("ERROR: Alibaba Cloud Translation network error%1").arg(serverMessage));
        return QSL("ERROR:TRAN_ALI_NETWORK_ERROR");
    }

    if (!doc.isObject()) {
        setErrorMsg(QSL("ERROR: Alibaba Cloud Translation JSON generic error"));
        return QSL("ERROR:TRAN_ALI_JSON_ERROR");
    }

    QJsonObject obj = doc.object();

    QString code = obj.value(QSL("Code")).toString();
    QString message = obj.value(QSL("Message")).toString();
    if (code != QSL("200")) {
        setErrorMsg(tr("ERROR: Alibaba Cloud Translation JSON error\n #%1: %2")
                    .arg(code,message));
        return QSL("ERROR:TRAN_ALI_JSON_ERROR");
    }

    const QString notPresentMarker = QSL("\t\r\n");
    QString res = obj.value(QSL("Data")).toObject().value(QSL("Translated")).toString(notPresentMarker);
    if (res == notPresentMarker) {
        setErrorMsg(tr("ERROR: Alibaba Cloud Translation JSON error - no translated text, no error"));
        return QSL("ERROR:TRAN_ALI_JSON_ERROR");
    }
    return res;
}
