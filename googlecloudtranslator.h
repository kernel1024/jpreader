#ifndef GOOGLECLOUDTRANSLATOR_H
#define GOOGLECLOUDTRANSLATOR_H

#include "structures.h"
#include "webapiabstracttranslator.h"

class CGoogleCloudTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT

private:
    QString m_authHeader;
    QString m_gcpProject;
    QString m_gcpEmail;
    QString m_gcpPrivateKey;

    bool parseJsonKey(const QString &jsonKeyFile);

protected:
    QString tranStringInternal(const QString& src) override;
    void clearCredentials() override;
    bool isValidCredentials() override;

public:
    CGoogleCloudTranslator(QObject *parent, const CLangPair& lang, const QString& jsonKeyFile);

    bool initTran() override;
    CStructures::TranslationEngine engine() override;

};

#endif // GOOGLECLOUDTRANSLATOR_H
