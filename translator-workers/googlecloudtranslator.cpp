#include <QUrlQuery>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "googlecloudtranslator.h"
#include "utils/genericfuncs.h"

CGoogleCloudTranslator::CGoogleCloudTranslator(QObject *parent, const CLangPair &lang, const QString &jsonKeyFile)
    : CWebAPIAbstractTranslator(parent, lang)
{
    clearCredentials();

    if (!parseJsonKey(jsonKeyFile))
        m_gcpPrivateKey.clear();
}

bool CGoogleCloudTranslator::parseJsonKey(const QString &jsonKeyFile)
{
    QFile f(jsonKeyFile);
    if (!f.open(QIODevice::ReadOnly)) {
        qCritical() << tr("ERROR: Google Cloud Translation JSON cannot open key file %1").arg(jsonKeyFile);
        return false;
    }

    QJsonParseError err {};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(),&err);
    f.close();
    if (doc.isNull()) {
        qCritical() << tr("ERROR: Google Cloud Translation JSON key parsing error #%1: %2")
                       .arg(err.error)
                       .arg(err.offset);
        return false;
    }

    m_gcpProject = doc.object().value(QSL("project_id")).toString();
    m_gcpEmail = doc.object().value(QSL("client_email")).toString();
    m_gcpPrivateKey = doc.object().value(QSL("private_key")).toString();

    return true;
}

bool CGoogleCloudTranslator::initTran()
{
    if (m_gcpPrivateKey.isEmpty())
        return false;

    m_authHeader.clear();

    const QString scope = QSL("https://www.googleapis.com/auth/cloud-platform "
                              "https://www.googleapis.com/auth/cloud-translation");
    const QString aud = QSL("https://oauth2.googleapis.com/token");

    const QDateTime time = QDateTime::currentDateTimeUtc();
    const QString iat = QSL("%1").arg(time.toSecsSinceEpoch());
    const QString exp = QSL("%1").arg(time.addSecs(CDefaults::oneHour).toSecsSinceEpoch());

    const QString jwtHeader = QSL(R"({"alg":"RS256","typ":"JWT"})");
    const QString jwtClaimSet = QSL(R"({"iss":"%1","scope":"%2","aud":"%3","exp":%4,"iat":%5})")
                                .arg(m_gcpEmail,scope,aud,exp,iat);

    QByteArray jwt = jwtHeader.toUtf8().toBase64(QByteArray::Base64UrlEncoding);
    jwt.append(u'.');
    jwt.append(jwtClaimSet.toUtf8().toBase64(QByteArray::Base64UrlEncoding));

    QByteArray sign = CGenericFuncs::signSHA256withRSA(jwt,m_gcpPrivateKey.toLatin1());
    if (sign.isEmpty())
        return false;

    jwt.append(u'.');
    jwt.append(sign.toBase64(QByteArray::Base64UrlEncoding));

    initNAM();

    QUrlQuery uq;
    uq.addQueryItem(QSL("grant_type"),QSL("urn:ietf:params:oauth:grant-type:jwt-bearer"));
    uq.addQueryItem(QSL("assertion"),QString::fromLatin1(jwt));
    QUrl rqurl = QUrl(aud);
    rqurl.setQuery(uq);
    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Content-Type","application/x-www-form-urlencoded");

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [rq]() -> QNetworkRequest {
        return rq;
    };
    QByteArray ra = processRequest(requestMaker,QByteArray(),&status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: Google Cloud Translation auth aborted by user request"));
        return false;
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(QSL("ERROR: Google Cloud Translation auth error"));
        return false;
    }

    QJsonParseError err {};
    QJsonDocument doc = QJsonDocument::fromJson(ra,&err);
    if (doc.isNull()) {
        qCritical() << tr("ERROR: Google Cloud Translation auth JSON reply error #%1: %2")
                       .arg(err.error)
                       .arg(err.offset);
        return false;
    }

    m_authHeader = QSL("Bearer %1").arg(doc.object().value(QSL("access_token")).toString());

    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine CGoogleCloudTranslator::engine()
{
    return CStructures::teGoogleCloud;
}

QString CGoogleCloudTranslator::tranStringInternal(const QString &src)
{
    if (m_gcpProject.isEmpty()) {
        setErrorMsg(QSL("ERROR: Google Cloud Translation JSON key parsing failure"));
        return QSL("ERROR:TRAN_GCP_KEY_FAILURE");
    }
    if (m_authHeader.isEmpty()) {
        setErrorMsg(QSL("ERROR: Google Cloud Translation auth failure"));
        return QSL("ERROR:TRAN_GCP_AUTH_FAILURE");
    }

    QUrl rqurl = QUrl(QSL("https://translation.googleapis.com/v3/projects/%1:translateText").arg(m_gcpProject));

    QJsonObject reqtext;
    reqtext[QSL("contents")] = QJsonArray( { src } );
    reqtext[QSL("sourceLanguageCode")] = language().langFrom.bcp47Name();
    reqtext[QSL("targetLanguageCode")] = language().langTo.bcp47Name();
    QJsonDocument doc(reqtext);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Authorization",m_authHeader.toUtf8());
    rq.setRawHeader("Content-Type","application/json; charset=utf-8");
    rq.setRawHeader("Content-Length",QString::number(body.size()).toLatin1());

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [rq]() -> QNetworkRequest {
        return rq;
    };
    QByteArray ra = processRequest(requestMaker,body,&status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: Google Cloud Translation aborted by user request"));
        return QSL("ERROR:TRAN_GCP_ABORTED");
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(QSL("ERROR: Google Cloud Translation network error"));
        return QSL("ERROR:TRAN_GCP_NETWORK_ERROR");
    }

    doc = QJsonDocument::fromJson(ra);
    if (doc.isNull()) {
        setErrorMsg(QSL("ERROR: Google Cloud Translation JSON error"));
        return QSL("ERROR:TRAN_GCP_JSON_ERROR");
    }

    if (!doc.isObject()) {
        setErrorMsg(QSL("ERROR: Google Cloud Translation JSON generic error"));
        return QSL("ERROR:TRAN_GCP_JSON_ERROR");
    }

    QJsonValue err = doc.object().value(QSL("error"));
    if (err.isObject()) {
        setErrorMsg(tr("ERROR: Google Cloud Translation JSON error #%1: %2")
                    .arg(err.toObject().value(QSL("code")).toInt())
                    .arg(err.toObject().value(QSL("message")).toString()));
        return QSL("ERROR:TRAN_GCP_JSON_ERROR");
    }

    QString res;
    const QJsonArray translist = doc.object().value(QSL("translations")).toArray();
    for (const QJsonValue &tv : qAsConst(translist)) {
        if (tv.toObject().contains(QSL("translatedText"))) {
            res+=tv.toObject().value(QSL("translatedText")).toString();
        }
    }
    return res;
}

void CGoogleCloudTranslator::clearCredentials()
{
    m_authHeader.clear();
}

bool CGoogleCloudTranslator::isValidCredentials()
{
    return !m_authHeader.isEmpty();
}
