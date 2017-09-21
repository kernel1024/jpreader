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

    virtual QString tranStringInternal(const QString& src) = 0;
    virtual void clearCredentials() = 0;
    virtual bool isValidCredentials() = 0;

private:
    void deleteNAM();

public:
    CWebAPIAbstractTranslator(QObject *parent, const QString &SrcLang);
    ~CWebAPIAbstractTranslator();

    QString tranString(const QString& src);
    void doneTran(bool lazyClose = false);
    bool isReady();

};

#endif // WEBAPIABSTRACTTRANSLATOR_H
