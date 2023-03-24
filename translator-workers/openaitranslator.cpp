#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "openaitranslator.h"
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/network.h"


// We are using only GPT-3 and GPT-4 chat models.
// Other completion models producing unacceptable garbage.

COpenAITranslator::COpenAITranslator(QObject *parent, const CLangPair &lang, const QString &translationModel,
                                     const QString &apiKey, double temperature,
                                     double topP, double presencePenalty, double frequencyPenalty)
    : CWebAPIAbstractTranslator(parent,lang),
      m_apiKey(apiKey),
      m_translationModel(translationModel),
      m_temperature(temperature),
      m_topP(topP),
      m_presencePenalty(presencePenalty),
      m_frequencyPenalty(frequencyPenalty)
{
}

QString COpenAITranslator::tranStringInternal(const QString &src)
{
    const QUrl rqurl(QSL("https://api.openai.com/v1/chat/completions"));

    const QString langFrom = QLocale::languageToString(language().langFrom.language());
    const QString langTo = QLocale::languageToString(language().langTo.language());

    const QString apiKey = QSL("Bearer %1").arg(m_apiKey);
    QNetworkRequest rq(rqurl);
    rq.setRawHeader("Content-Type","application/json");
    rq.setRawHeader("Authorization",apiKey.toLatin1());

    // Necessary parameters
    QJsonObject reqtext;
    reqtext[QSL("model")] = m_translationModel;

    QJsonArray messages;
    messages.append(QJsonObject( {
                                     { QSL("role"), QSL("user") },
                                     { QSL("content"), QSL("Translate this from %1 into %2:").arg(langFrom,langTo) }
                                 } ));
    messages.append(QJsonObject( {
                                     { QSL("role"), QSL("user") },
                                     { QSL("content"), src }
                                 } ));

    reqtext[QSL("messages")] = messages;

    // Optional generation parameters
    reqtext[QSL("temperature")] = m_temperature;
    reqtext[QSL("top_p")] = m_topP;
    reqtext[QSL("presence_penalty")] = m_presencePenalty;
    reqtext[QSL("frequency_penalty")] = m_frequencyPenalty;

    const QJsonDocument doc(reqtext);
    const QByteArray body = doc.toJson(QJsonDocument::Compact);

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    auto requestMaker = [rq]() -> QNetworkRequest {
        return rq;
    };
    const QByteArray ra = processRequest(requestMaker,body,&status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: OpenAI translator aborted by user request"));
        return QSL("ERROR:OPENAI_ABORTED");
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(tr("ERROR: OpenAI translator network error"));
        return QSL("ERROR:OPENAI_NETWORK_ERROR");
    }

    const QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        setErrorMsg(tr("ERROR: OpenAI translator JSON error"));
        return QSL("ERROR:OPENAI_JSON_ERROR");
    }

    const QJsonObject jroot = jdoc.object();
    const QJsonValue err = jroot.value(QSL("error"));
    if (err.isObject()) {
        const QJsonValue errMsg = err.toObject().value(QSL("message"));
        if (errMsg.isString()) {
            setErrorMsg(tr("ERROR: OpenAI translator error %1, HTTP status: %2")
                        .arg(errMsg.toString()).arg(status));
            return QSL("ERROR:OPENAI_JSON_ERROR");
        }

        if (status!=CDefaults::httpCodeFound) {
            setErrorMsg(QSL("ERROR: OpenAI translator HTTP generic error, HTTP status: %1").arg(status));
            return QSL("ERROR:OPENAI_HTTP_ERROR");
        }

        setErrorMsg(tr("ERROR: OpenAI unknown error."));
        return QSL("ERROR:OPENAI_UNKNOWN_ERROR");
    }

    const QJsonArray choices = jroot.value(QSL("choices")).toArray();
    if (choices.isEmpty()) {
        setErrorMsg(QSL("ERROR: OpenAI translator 'choices' member missing from JSON response, "
                        "HTTP status: %1").arg(status));
        return QSL("ERROR:OPENAI_RESPONSE_ERROR");
    }

    QString res;
    for (const auto& t : choices) {
        const QJsonValue text = t.toObject().value(QSL("message")).toObject().value(QSL("content"));
        if (text.isString())
            res.append(text.toString());
    }
    return res;
}

bool COpenAITranslator::isValidCredentials()
{
    return (!m_apiKey.isEmpty() && !m_translationModel.isEmpty());
}

bool COpenAITranslator::initTran()
{
    initNAM();

    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine COpenAITranslator::engine()
{
    return CStructures::teOpenAI;
}

QStringList COpenAITranslator::getAvailableModels(const QString &apiKey)
{
    Q_UNUSED(apiKey)

    // TODO: also collect and add up to 10 translation samples to translation prompt (for consistency).

    // TODO: use apiKey for https://api.openai.com/v1/models

    // the first one is default model
    static const QStringList models( { QSL("gpt-3.5-turbo"), QSL("gpt-4") } );

    return models;
}

QString COpenAITranslator::getModelName() const
{
    return m_translationModel;
}
