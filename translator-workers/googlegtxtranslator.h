#ifndef GOOGLEGTXTRANSLATOR_H
#define GOOGLEGTXTRANSLATOR_H

#include "webapiabstracttranslator.h"

class CGoogleGTXTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
protected:
    QString tranStringInternal(const QString& src) override;
    bool isValidCredentials() override;

public:
    CGoogleGTXTranslator(QObject *parent, const CLangPair &lang);

    bool initTran() override;
    CStructures::TranslationEngine engine() override;

};

#endif // GOOGLEGTXTRANSLATOR_H
