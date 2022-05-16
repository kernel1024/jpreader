#ifndef CPROMTNMTTRANSLATOR_H
#define CPROMTNMTTRANSLATOR_H

#include <QObject>
#include "webapiabstracttranslator.h"

class CPromtNmtTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
private:
    QString m_apiKey;
    QString m_serverUrl;
    QUrl queryUrl(const QString& method) const;

protected:
    QString tranStringInternal(const QString& src) override;
    bool isValidCredentials() override;

public:
    CPromtNmtTranslator(QObject *parent, const CLangPair &lang, const QString& serverUrl,
                        const QString& apiKey);

    bool initTran() override;
    CStructures::TranslationEngine engine() override;

};

#endif // CPROMTNMTTRANSLATOR_H
