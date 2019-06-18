#include <QUrlQuery>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkReply>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
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

QByteArray CAWSTranslator::getSignatureKey(const QString &key, const QString &dateStamp,
                                      const QString &regionName, const QString &serviceName) const
{
    QByteArray bKey = QStringLiteral("AWS4%1").arg(key).toUtf8();
    QByteArray kDate = QMessageAuthenticationCode::hash(dateStamp.toUtf8(),bKey,QCryptographicHash::Sha256);
    QByteArray kRegion = QMessageAuthenticationCode::hash(regionName.toUtf8(),kDate,QCryptographicHash::Sha256);
    QByteArray kService = QMessageAuthenticationCode::hash(serviceName.toUtf8(),kRegion,QCryptographicHash::Sha256);
    QByteArray kSigning = QMessageAuthenticationCode::hash("aws4_request",kService,QCryptographicHash::Sha256);
    return kSigning;
}

QString CAWSTranslator::tranStringInternal(const QString &src)
{
    QJsonObject reqtext;
    reqtext[QStringLiteral("Text")] = src;
    reqtext[QStringLiteral("SourceLanguageCode")] = language().langFrom.bcp47Name();
    reqtext[QStringLiteral("TargetLanguageCode")] = language().langTo.bcp47Name();
    QJsonDocument doc(reqtext);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);


    const int httpUnknownErrorCode = 499;
    const QString service("translate");
    const QString algorithm("AWS4-HMAC-SHA256");
    const QString signedHeaders = QStringLiteral("content-type;host;x-amz-date;x-amz-target");
    const QString amz_target("AWSShineFrontendService_20170701.TranslateText");
    const QString contentType("application/x-amz-json-1.1");

    QString host = QStringLiteral("%1.%2.amazonaws.com").arg(service,m_region);
    QDateTime dt = QDateTime::currentDateTimeUtc();
    QString amz_date = dt.toString(QStringLiteral("yyyyMMddThhmmssZ"));
    QString dateStamp = dt.toString("yyyyMMdd");

    QString credentialScope = QStringLiteral("%1/%2/%3/aws4_request").arg(
                                    dateStamp,
                                    m_region,
                                    service);

    QString canonicalHeaders = QStringLiteral("content-type:%1\n"
                                              "host:%2\n"
                                              "x-amz-date:%3\n"
                                              "x-amz-target:%4\n").arg(
                                   contentType,
                                   host,
                                   amz_date,
                                   amz_target);

    QString payloadHash = QString::fromLatin1(QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex());

    QString canonicalRequest = QStringLiteral("%1\n%2\n%3\n%4\n%5\n%6").arg(
                                   QStringLiteral("POST"), // method
                                   QStringLiteral("/"), // canonical_uri
                                   QString(), // canonical_querystring
                                   canonicalHeaders, // canonical_headers
                                   signedHeaders, // signed_headers
                                   payloadHash); // payload_hash

    QString canonicalRequestDigest = QString::fromLatin1(QCryptographicHash::hash(canonicalRequest.toUtf8(),
                                                                                  QCryptographicHash::Sha256).toHex());

    QString stringToSign = QStringLiteral("%1\n%2\n%3\n%4").arg(algorithm,amz_date,
                                                                credentialScope,canonicalRequestDigest);

    QString signature = QString::fromLatin1(QMessageAuthenticationCode::hash(stringToSign.toUtf8(),
                                                                             getSignatureKey(m_secretKey,
                                                                                             dateStamp,
                                                                                             m_region,
                                                                                             service),
                                                                             QCryptographicHash::Sha256).toHex());

    QByteArray authorization_header = QString("%1 Credential=%2/%3, "
                                              "SignedHeaders=%4, "
                                              "Signature=%5").arg(algorithm,m_accessKey,credentialScope,
                                                                  signedHeaders,
                                                                  signature).toLatin1();


    QUrl rqurl = QUrl(QStringLiteral("https://%1/").arg(host));

    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Content-Type",contentType.toLatin1());
    rq.setRawHeader("X-Amz-Date",amz_date.toLatin1());
    rq.setRawHeader("X-Amz-Target",amz_target.toLatin1());
    rq.setRawHeader("Authorization",authorization_header);

    QNetworkReply *rpl = nam()->post(rq,payload);

    if (!waitForReply(rpl)) {
        setErrorMsg(QStringLiteral("ERROR: AWS translator network error"));
        return QStringLiteral("ERROR:TRAN_AWS_NETWORK_ERROR");
    }

    QByteArray ra = rpl->readAll();

    QVariant vstatus = rpl->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    bool ok;
    int status = vstatus.toInt(&ok);
    if (!ok)
        status = httpUnknownErrorCode;

    rpl->close();

    doc = QJsonDocument::fromJson(ra);
    if (doc.isNull()) {
        setErrorMsg(QStringLiteral("ERROR: AWS translator JSON error, HTTP status: %1").arg(status));
        return QStringLiteral("ERROR:TRAN_AWS_JSON_ERROR");
    }

    QJsonObject vobj = doc.object();
    QJsonValue err = vobj.value(QStringLiteral("message"));
    QJsonValue errType = vobj.value(QStringLiteral("__type"));
    if (err.isString()) {
        setErrorMsg(tr("ERROR: AWS translator JSON error #%1: %2, HTTP status: %3")
                    .arg(errType.toString())
                    .arg(err.toString())
                    .arg(status));
        return QStringLiteral("ERROR:TRAN_AWS_JSON_ERROR");
    }

    if (status!=200) {
        setErrorMsg(QStringLiteral("ERROR: AWS translator HTTP generic error, HTTP status: %1").arg(status));
        return QStringLiteral("ERROR:TRAN_AWS_HTTP_ERROR");
    }

    QJsonValue vres = vobj.value(QStringLiteral("TranslatedText"));
    if (vres.isString())
        return vres.toString();

    setErrorMsg(QStringLiteral("ERROR: AWS translator TranslatedText missing from JSON response, HTTP status: %1").arg(status));
    return QStringLiteral("ERROR:TRAN_AWS_RESPONSE_ERROR");
}

void CAWSTranslator::clearCredentials()
{
}

bool CAWSTranslator::isValidCredentials()
{
    return (!m_region.isEmpty() && !m_accessKey.isEmpty() && !m_secretKey.isEmpty());
}
