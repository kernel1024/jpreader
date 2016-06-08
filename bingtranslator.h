#ifndef CBINGTRANSLATOR_H
#define CBINGTRANSLATOR_H

#include <QNetworkAccessManager>
#include "abstracttranslator.h"

class CBingTranslator : public CAbstractTranslator
{
private:
    QNetworkAccessManager *nam;
    QString clientID, clientKey;
    QString authHeader;

    bool waitForReply(QNetworkReply* reply);

    QString tranStringInternal(QString src);

public:
    CBingTranslator(QObject *parent, const QString& SrcLang, const QString& bingID, const QString& bingKey);
    ~CBingTranslator();

    bool initTran();
    QString tranString(QString src);
    void doneTran(bool lazyClose = false);
    bool isReady();

};

#endif // CBINGTRANSLATOR_H
