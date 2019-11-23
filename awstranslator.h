#ifndef AWSTRANSLATOR_H
#define AWSTRANSLATOR_H

#include <QObject>
#include "structures.h"
#include "webapiabstracttranslator.h"

namespace CDefaults {
const int awsMaxRetries = 10;
const unsigned int awsMinRetryDelay = 10;
const unsigned int awsMaxRetryDelay = 25;
}

class CAWSTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
public:
    CAWSTranslator(QObject *parent, const CLangPair& lang, const QString& region, const QString& accessKey,
                   const QString& secretKey);

    bool initTran() override;
    CStructures::TranslationEngine engine() override;

private:
    QString m_region;
    QString m_accessKey;
    QString m_secretKey;

    QByteArray getSignatureKey(const QString& key, const QString& dateStamp,
                             const QString& regionName, const QString& serviceName) const;
    QNetworkRequest createAWSRequest(const QString &service, const QString &amz_target,
                                     const CStringHash &additionalHeaders, const QByteArray &payload);

protected:
    QString tranStringInternal(const QString& src) override;
    void clearCredentials() override;
    bool isValidCredentials() override;

};

#endif // AWSTRANSLATOR_H
