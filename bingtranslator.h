#ifndef CBINGTRANSLATOR_H
#define CBINGTRANSLATOR_H

#include "webapiabstracttranslator.h"

class CBingTranslator : public CWebAPIAbstractTranslator
{
protected:
    QString clientKey;
    QString authHeader;

    QString tranStringInternal(const QString& src);
    void clearCredentials();
    bool isValidCredentials();

public:
    CBingTranslator(QObject *parent, const QString& SrcLang, const QString& bingKey);

    bool initTran();

};

#endif // CBINGTRANSLATOR_H
