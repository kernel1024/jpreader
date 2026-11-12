#ifndef OLLAMATRANSLATOR_H
#define OLLAMATRANSLATOR_H

#include <QObject>
#include <QUrl>
#include <QString>
#include "webapiabstracttranslator.h"

class COllamaTranslator : public CWebAPIAbstractTranslator
{
    Q_OBJECT
private:
    QString m_model;
    QUrl m_baseUrl;

public:
    COllamaTranslator(QObject *parent, const CLangPair &lang, const QUrl &baseUrl,
                      const QString &model);
    bool initTran() override;
    CStructures::TranslationEngine engine() override;
    QString getModelName() const override;

protected:
    QString tranStringInternal(const QString& src) override;
    bool isValidCredentials() override;
};

#endif // OLLAMATRANSLATOR_H
