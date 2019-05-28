#ifndef CBINGTRANSLATOR_H
#define CBINGTRANSLATOR_H

#include "webapiabstracttranslator.h"

class CBingTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
private:
    QString clientKey;
    QString authHeader;

protected:
    QString tranStringInternal(const QString& src) override;
    void clearCredentials() override;
    bool isValidCredentials() override;

public:
    CBingTranslator(QObject *parent, const CLangPair& lang, const QString& bingKey);

    bool initTran() override;

};

#endif // CBINGTRANSLATOR_H
