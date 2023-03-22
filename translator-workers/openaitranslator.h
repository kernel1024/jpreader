#ifndef COPENAITRANSLATOR_H
#define COPENAITRANSLATOR_H

#include <QObject>
#include "webapiabstracttranslator.h"
#include "global/settings.h"


class COpenAITranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT

    QString m_apiKey;
    QString m_translationModel;

    double m_temperature { CDefaults::openaiTemperature };
    double m_topP { CDefaults::openaiTopP };
    double m_presencePenalty { CDefaults::openaiPresencePenalty };
    double m_frequencyPenalty { CDefaults::openaiFrequencyPenalty };

public:
    COpenAITranslator(QObject *parent, const CLangPair &lang, const QString &translationModel,
                      const QString &apiKey, double temperature = CDefaults::openaiTemperature,
                      double topP = CDefaults::openaiTopP, double presencePenalty = CDefaults::openaiPresencePenalty,
                      double frequencyPenalty = CDefaults::openaiFrequencyPenalty);
    bool initTran() override;
    CStructures::TranslationEngine engine() override;

    static int tokensCount(const QString& text);
    static QStringList getAvailableModels(const QString &apiKey);


protected:
    QString tranStringInternal(const QString& src) override;
    bool isValidCredentials() override;

private:
};

#endif // COPENAITRANSLATOR_H
