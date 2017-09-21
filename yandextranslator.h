#ifndef CYANDEXTRANSLATOR_H
#define CYANDEXTRANSLATOR_H

#include "webapiabstracttranslator.h"

class CYandexTranslator : public CWebAPIAbstractTranslator
{
private:
    QString clientKey;

    QString tranStringInternal(const QString& src);
    void clearCredentials();
    bool isValidCredentials();

public:
    CYandexTranslator(QObject *parent, const QString& SrcLang, const QString& yandexKey);

    bool initTran();

};

#endif // CYANDEXTRANSLATOR_H
