#ifndef GOOGLEGTXTRANSLATOR_H
#define GOOGLEGTXTRANSLATOR_H

#include "webapiabstracttranslator.h"

class CGoogleGTXTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
protected:
    QString tranStringInternal(const QString& src) override;
    void clearCredentials() override;
    bool isValidCredentials() override;

public:
    CGoogleGTXTranslator(QObject *parent, const CLangPair &lang);

    bool initTran() override;
};

#endif // GOOGLEGTXTRANSLATOR_H
