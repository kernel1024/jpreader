#ifndef AWSTRANSLATOR_H
#define AWSTRANSLATOR_H

#include <QObject>
#include "webapiabstracttranslator.h"

class CAWSTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
public:
    CAWSTranslator(QObject *parent, const CLangPair& lang, const QString& region, const QString& accessKey,
                   const QString& secretKey);

    bool initTran() override;

private:
    QString m_region;
    QString m_accessKey;
    QString m_secretKey;

    QByteArray getSignatureKey(const QString& key, const QString& dateStamp,
                             const QString& regionName, const QString& serviceName) const;

protected:
    QString tranStringInternal(const QString& src) override;
    void clearCredentials() override;
    bool isValidCredentials() override;

};

#endif // AWSTRANSLATOR_H
