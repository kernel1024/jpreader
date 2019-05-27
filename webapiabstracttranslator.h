#ifndef WEBAPIABSTRACTTRANSLATOR_H
#define WEBAPIABSTRACTTRANSLATOR_H

#include <QNetworkAccessManager>
#include <QString>
#include "abstracttranslator.h"

class CWebAPIAbstractTranslator : public CAbstractTranslator
{
protected:
    QNetworkAccessManager *nam;

    bool waitForReply(QNetworkReply* reply);
    void initNAM();

    virtual QString tranStringInternal(const QString& src) = 0;
    virtual void clearCredentials() = 0;
    virtual bool isValidCredentials() = 0;

private:
    void deleteNAM();

public:
    CWebAPIAbstractTranslator(QObject *parent, const CLangPair &lang);
    ~CWebAPIAbstractTranslator() override;

    QString tranString(const QString& src) override;
    void doneTran(bool lazyClose = false) override;
    bool isReady() override;

};

#endif // WEBAPIABSTRACTTRANSLATOR_H
