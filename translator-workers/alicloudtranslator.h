#ifndef ALICLOUDTRANSLATOR_H
#define ALICLOUDTRANSLATOR_H

#include <QObject>
#include <QNetworkRequest>
#include "global/structures.h"
#include "webapiabstracttranslator.h"

class CAliCloudTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
private:
    CStructures::AliCloudTranslatorMode m_mode { CStructures::aliTranslatorGeneral };
    QString m_accessKeyID;
    QString m_accessKeySecret;

    QNetworkRequest createAliRequest(const CStringHash &body) const;
    QString canonicalizedQuery(const CStringHash& params) const;

protected:
    QString tranStringInternal(const QString& src) override;
    bool isValidCredentials() override;

public:
    CAliCloudTranslator(QObject *parent, const CLangPair& lang,
                        CStructures::AliCloudTranslatorMode mode,
                        const QString& accessKeyID,
                        const QString& accessKeySecret);
    bool initTran() override;
    CStructures::TranslationEngine engine() override;

};

#endif // ALICLOUDTRANSLATOR_H
