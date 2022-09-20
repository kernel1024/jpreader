#ifndef CDEEPLAPITRANSLATOR_H
#define CDEEPLAPITRANSLATOR_H

#include <QObject>
#include "webapiabstracttranslator.h"

class CDeeplAPITranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT

private:
    QString m_apiKey;
    CStructures::DeeplAPIMode m_apiMode;
    CStructures::DeeplAPISplitSentences m_splitSentences;
    CStructures::DeeplAPIFormality m_formality;
    QUrl queryUrl(const QString &method) const;

protected:
    QString tranStringInternal(const QString& src) override;
    bool isValidCredentials() override;

public:
    CDeeplAPITranslator(QObject *parent, const CLangPair &lang, CStructures::DeeplAPIMode apiMode,
                        const QString &apiKey, CStructures::DeeplAPISplitSentences splitSentences,
                        CStructures::DeeplAPIFormality formality);
    bool initTran() override;
    CStructures::TranslationEngine engine() override;

};

#endif // CDEEPLAPITRANSLATOR_H
