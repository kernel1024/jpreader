#ifndef CYANDEXCLOUDTRANSLATOR_H
#define CYANDEXCLOUDTRANSLATOR_H

#include <QObject>
#include "webapiabstracttranslator.h"

class CYandexCloudTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
private:
    QString m_apiKey;
    QString m_folderID;

protected:
    QString tranStringInternal(const QString& src) override;
    void clearCredentials() override;
    bool isValidCredentials() override;

public:
    CYandexCloudTranslator(QObject *parent, const CLangPair &lang, const QString& apiKey,
                           const QString& folderID);

    bool initTran() override;
    CStructures::TranslationEngine engine() override;


};

#endif // CYANDEXCLOUDTRANSLATOR_H
