#include <QJsonObject>
#include "ollamatranslator.h"
#include "../utils/genericfuncs.h"

COllamaTranslator::COllamaTranslator(QObject *parent, const CLangPair &lang,
                                     const QUrl &baseUrl, const QString &model)
    : CWebAPIAbstractTranslator(parent,lang),
    m_model(model),
    m_baseUrl(baseUrl)
{
}

QString COllamaTranslator::tranStringInternal(const QString &src)
{
    const QString langFrom = QLocale::languageToString(language().langFrom.language());
    const QString langTo = QLocale::languageToString(language().langTo.language());

    QJsonObject request;

    request[QSL("model")] = m_model;
    request[QSL("stream")] = false;

    request[QSL("prompt")] =
        QString(
            QSL("Translate the following text from %1 to %2. "
                "Preserve the meaning and dialogue structure. "
                "Return only the translation.\n\n%3")
            ).arg(langFrom, langTo, src);

    QNetworkRequest rq(m_baseUrl.resolved(QUrl(QSL("/api/generate"))));

    rq.setHeader(QNetworkRequest::ContentTypeHeader, QSL("application/json"));

    bool aborted = false;
    int status = CDefaults::httpCodeClientUnknownError;
    const auto requestMaker = [rq]() -> QNetworkRequest {
        return rq;
    };
    const QByteArray ra = processRequest(requestMaker,QJsonDocument(request).toJson(QJsonDocument::Compact),&status,&aborted);

    if (aborted) {
        setErrorMsg(QSL("ERROR: Ollama translator aborted by user request"));
        return QSL("ERROR:OLLAMA_ABORTED");
    }
    if (ra.isEmpty() || status>=CDefaults::httpCodeClientError) {
        setErrorMsg(tr("ERROR: Ollama translator network error"));
        return QSL("ERROR:OLLAMA_NETWORK_ERROR");
    }

    const QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty()) {
        setErrorMsg(tr("ERROR: Ollama translator JSON error"));
        return QSL("ERROR:OLLAMA_JSON_ERROR");
    }

    if (!jdoc.object().contains(QSL("response"))) {
        setErrorMsg(QSL("ERROR: Ollama translator 'response' member missing from JSON response, "
                        "HTTP status: %1").arg(status));
        return QSL("ERROR:OLLAMA_RESPONSE_ERROR");
    }

    const QString res = jdoc.object().value(QSL("response")).toString();
    return res;
}

bool COllamaTranslator::isValidCredentials()
{
    // Configuration readiness, not an HTTP server health check.
    return m_baseUrl.isValid()
           && (m_baseUrl.scheme() == QStringLiteral("http")
               || m_baseUrl.scheme() == QStringLiteral("https"))
           && !m_baseUrl.host().isEmpty() && !m_model.trimmed().isEmpty();
}

bool COllamaTranslator::initTran()
{
    initNAM();

    clearErrorMsg();
    return true;
}

CStructures::TranslationEngine COllamaTranslator::engine()
{
    return CStructures::teOllama;
}

QString COllamaTranslator::getModelName() const
{
    return m_model;
}
