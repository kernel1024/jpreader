#ifndef CYANDEXTRANSLATOR_H
#define CYANDEXTRANSLATOR_H

#include <QNetworkAccessManager>
#include <QString>
#include "abstracttranslator.h"

class CYandexTranslator : public CAbstractTranslator
{
private:
    QNetworkAccessManager *nam;
    QString clientKey;
    QString srcLang;

    bool waitForReply(QNetworkReply* reply);

    QString tranStringInternal(QString src);

public:
    CYandexTranslator(QObject *parent, const QString& yandexKey);
    ~CYandexTranslator();

    bool initTran();
    QString tranString(QString src);
    void doneTran(bool lazyClose = false);
    bool isReady();

};

#endif // CYANDEXTRANSLATOR_H
