#ifndef GOOGLEGTXTRANSLATOR_H
#define GOOGLEGTXTRANSLATOR_H

#include "webapiabstracttranslator.h"

class CGoogleGTXTranslator : public CWebAPIAbstractTranslator
{
protected:
    QString tranStringInternal(const QString& src);
    void clearCredentials();
    bool isValidCredentials();

public:
    CGoogleGTXTranslator(QObject *parent, const CLangPair &lang);

    bool initTran();
};

#endif // GOOGLEGTXTRANSLATOR_H
