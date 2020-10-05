#include <QUrlQuery>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QThread>
#include <algorithm>
#include "awstranslator.h"

CAWSTranslator::CAWSTranslator(QObject *parent, const CLangPair &lang, const QString &region,
                               const QString &accessKey, const QString &secretKey)
    : CWebAPIAbstractTranslator(parent,lang)
{
    m_region = region;
    m_accessKey = accessKey;
    m_secretKey = secretKey;
    clearCredentials();
}

bool CAWSTranslator::initTran()
{
    initNAM();
    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine CAWSTranslator::engine()
{
    return CStructures::teAmazonAWS;
}

QByteArray CAWSTranslator::getSignatureKey(const QString &key, const QString &dateStamp,
                                           const QString &regionName, const QString &serviceName) const
{
    QByteArray bKey = QSL("AWS4%1").arg(key).toUtf8();
    QByteArray kDate = QMessageAuthenticationCode::hash(dateStamp.toUtf8(),bKey,QCryptographicHash::Sha256);
    QByteArray kRegion = QMessageAuthenticationCode::hash(regionName.toUtf8(),kDate,QCryptographicHash::Sha256);
    QByteArray kService = QMessageAuthenticationCode::hash(serviceName.toUtf8(),kRegion,QCryptographicHash::Sha256);
    QByteArray kSigning = QMessageAuthenticationCode::hash("aws4_request",kService,QCryptographicHash::Sha256);
    return kSigning;
}

QNetworkRequest CAWSTranslator::createAWSRequest(const QString& service,
                                                 const QString& amz_target,
                                                 const CStringHash& additionalHeaders,
                                                 const QByteArray& payload)
{
    const QString algorithm(QSL("AWS4-HMAC-SHA256"));

    QString host = QSL("%1.%2.amazonaws.com").arg(service,m_region);
    QDateTime dt = QDateTime::currentDateTimeUtc();
    QString amz_date = dt.toString(QSL("yyyyMMddThhmmssZ"));
    QString dateStamp = dt.toString(QSL("yyyyMMdd"));

    QUrl rqurl = QUrl(QSL("https://%1/").arg(host));
    QNetworkRequest rq(rqurl);

    CStringHash headers = additionalHeaders;
    headers[QSL("host")] = host;
    headers[QSL("x-amz-date")] = amz_date;
    headers[QSL("x-amz-target")] = amz_target;

    QStringList headerNames;
    CStringHash lheaders;
    for (auto it = headers.constKeyValueBegin(), end = headers.constKeyValueEnd(); it!=end; ++it) {
        const QString lkey = (*it).first.toLower();
        lheaders[lkey] = (*it).second;
        headerNames.append(lkey);
        rq.setRawHeader((*it).first.toLatin1(),(*it).second.toLatin1());
    }
    std::sort(headerNames.begin(),headerNames.end());

    QString signedHeaders = headerNames.join(QChar(';'));

    QString canonicalHeaders;
    for (const auto& key : qAsConst(headerNames)) {
        canonicalHeaders.append(QSL("%1:%2\n").arg(key,lheaders.value(key)));
    }

    QString credentialScope = QSL("%1/%2/%3/aws4_request").arg(
                                  dateStamp,
                                  m_region,
                                  service);

    QString payloadHash = QString::fromLatin1(QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex());

    QString canonicalRequest = QSL("%1\n%2\n%3\n%4\n%5\n%6").arg(
                                   QSL("POST"), // method
                                   QSL("/"), // canonical_uri
                                   QString(), // canonical_querystring
                                   canonicalHeaders, // canonical_headers
                                   signedHeaders, // signed_headers
                                   payloadHash); // payload_hash

    QString canonicalRequestDigest = QString::fromLatin1(QCryptographicHash::hash(canonicalRequest.toUtf8(),
                                                                                  QCryptographicHash::Sha256).toHex());

    QString stringToSign = QSL("%1\n%2\n%3\n%4").arg(algorithm,amz_date,
                                                                credentialScope,canonicalRequestDigest);

    QString signature = QString::fromLatin1(QMessageAuthenticationCode::hash(stringToSign.toUtf8(),
                                                                             getSignatureKey(m_secretKey,
                                                                                             dateStamp,
                                                                                             m_region,
                                                                                             service),
                                                                             QCryptographicHash::Sha256).toHex());

    QByteArray authorization_header = QSL("%1 Credential=%2/%3, "
                                                     "SignedHeaders=%4, "
                                                     "Signature=%5").arg(algorithm,m_accessKey,credentialScope,
                                                                         signedHeaders,
                                                                         signature).toLatin1();

    rq.setRawHeader("Authorization",authorization_header);

    return rq;
}

QString CAWSTranslator::tranStringInternal(const QString &src)
{
    QJsonObject reqtext;
    reqtext[QSL("Text")] = src;
    reqtext[QSL("SourceLanguageCode")] = language().langFrom.bcp47Name();
    reqtext[QSL("TargetLanguageCode")] = language().langTo.bcp47Name();
    QJsonDocument doc(reqtext);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    const QString service(QSL("translate"));
    const QString amz_target(QSL("AWSShineFrontendService_20170701.TranslateText"));
    const QString contentType(QSL("application/x-amz-json-1.1"));

    const CStringHash signedHeaders = { { QSL("Content-Type"), contentType } };

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [this,service,amz_target,signedHeaders,payload]() -> QNetworkRequest {
        return createAWSRequest(service,amz_target,signedHeaders,payload);
    };
    QByteArray ra = processRequest(requestMaker,payload,&status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: AWS translator aborted by user request"));
        return QSL("ERROR:TRAN_AWS_ABORTED");
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(QSL("ERROR: AWS translator network error"));
        return QSL("ERROR:TRAN_AWS_NETWORK_ERROR");
    }

    doc = QJsonDocument::fromJson(ra);
    if (doc.isNull()) {
        setErrorMsg(QSL("ERROR: AWS translator JSON error, HTTP status: %1").arg(status));
        return QSL("ERROR:TRAN_AWS_JSON_ERROR");
    }

    QJsonObject vobj = doc.object();
    QJsonValue err = vobj.value(QSL("message"));
    QJsonValue errType = vobj.value(QSL("__type"));
    if (err.isString()) {
        setErrorMsg(tr("ERROR: AWS translator JSON error #%1: %2, HTTP status: %3")
                    .arg(errType.toString(),err.toString())
                    .arg(status));
        return QSL("ERROR:TRAN_AWS_JSON_ERROR");
    }

    if (status!=CDefaults::httpCodeFound) {
        setErrorMsg(QSL("ERROR: AWS translator HTTP generic error, HTTP status: %1").arg(status));
        return QSL("ERROR:TRAN_AWS_HTTP_ERROR");
    }

    QJsonValue vres = vobj.value(QSL("TranslatedText"));
    if (vres.isString())
        return vres.toString();

    setErrorMsg(QSL("ERROR: AWS translator TranslatedText missing from JSON response, HTTP status: %1").arg(status));
    return QSL("ERROR:TRAN_AWS_RESPONSE_ERROR");
}

bool CAWSTranslator::isValidCredentials()
{
    return (!m_region.isEmpty() && !m_accessKey.isEmpty() && !m_secretKey.isEmpty());
}
