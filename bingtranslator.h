#ifndef CBINGTRANSLATOR_H
#define CBINGTRANSLATOR_H

#include <QNetworkAccessManager>
#include "abstracttranslator.h"

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)

class CBingTranslator : public CAbstractTranslator
{
private:
    QNetworkAccessManager *nam;
    QString clientID, clientKey;
    QString authHeader;
    QString srcLang;

    bool waitForReply(QNetworkReply* reply);

    QString tranStringInternal(QString src);

public:
    CBingTranslator(QObject *parent, const QString& bingID, const QString& bingKey);
    ~CBingTranslator();

    bool initTran();
    QString tranString(QString src);
    void doneTran(bool lazyClose = false);
    bool isReady();

};

#endif // QT5

#endif // CBINGTRANSLATOR_H
