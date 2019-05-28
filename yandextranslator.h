#ifndef CYANDEXTRANSLATOR_H
#define CYANDEXTRANSLATOR_H

#include "webapiabstracttranslator.h"

class CYandexTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
private:
    QString clientKey;

protected:
    QString tranStringInternal(const QString& src) override;
    void clearCredentials() override;
    bool isValidCredentials() override;

public:
    CYandexTranslator(QObject *parent, const CLangPair &lang, const QString& yandexKey);

    bool initTran() override;

};

#endif // CYANDEXTRANSLATOR_H
