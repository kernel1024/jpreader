#include <QUrlQuery>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "googlecloudtranslator.h"

CGoogleCloudTranslator::CGoogleCloudTranslator(QObject *parent, const CLangPair &lang, const QString &jsonKeyFile)
    : CWebAPIAbstractTranslator(parent, lang)
{
    clearCredentials();

    m_clientJsonKeyFile = jsonKeyFile;
    if (!parseJsonKey())
        m_clientJsonKeyFile.clear();
}

bool CGoogleCloudTranslator::parseJsonKey()
{
    QFile f(m_clientJsonKeyFile);
    if (!f.open(QIODevice::ReadOnly)) {
        qCritical() << tr("ERROR: Google Cloud Translation JSON cannot open key file %1").arg(m_clientJsonKeyFile);
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(),&err);
    f.close();
    if (doc.isNull()) {
        qCritical() << tr("ERROR: Google Cloud Translation JSON key parsing error #%1: %2")
                       .arg(err.error)
                       .arg(err.offset);
        return false;
    }

    m_gcpProject = doc.object().value(QSL("project_id")).toString();
    return true;
}

bool CGoogleCloudTranslator::initTran()
{
    initNAM();
    m_authHeader.clear();

    const int gcloudTimeoutMS = 10000;

    // TODO: move away from subprocess to direct API access
    QProcess gcloud;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QSL("GOOGLE_APPLICATION_CREDENTIALS"),m_clientJsonKeyFile);
    gcloud.setProcessEnvironment(env);
    gcloud.start(QSL("gcloud"), { QSL("auth"), QSL("application-default"), QSL("print-access-token") } );
    gcloud.waitForFinished(gcloudTimeoutMS);

    QByteArray ra = gcloud.readAllStandardOutput().trimmed();
    if (!ra.isEmpty())
        m_authHeader = QSL("Bearer %1").arg(QString::fromUtf8(ra));

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
