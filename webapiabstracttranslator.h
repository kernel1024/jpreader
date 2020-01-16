#ifndef WEBAPIABSTRACTTRANSLATOR_H
#define WEBAPIABSTRACTTRANSLATOR_H

#include <QNetworkAccessManager>
#include <QString>
#include "abstracttranslator.h"

class CWebAPIAbstractTranslator : public CAbstractTranslator
{
    Q_OBJECT
protected:
    bool waitForReply(QNetworkReply* reply, int *httpStatus);
    void initNAM();
    QNetworkAccessManager* nam() { return m_nam; }

    virtual QString tranStringInternal(const QString& src) = 0;
    virtual void clearCredentials() = 0;
    virtual bool isValidCredentials() = 0;

private:
    QNetworkAccessManager *m_nam { nullptr };

    Q_DISABLE_COPY(CWebAPIAbstractTranslator)

    void deleteNAM();

public:
    CWebAPIAbstractTranslator(QObject *parent, const CLangPair &lang);
    ~CWebAPIAbstractTranslator() override;

    QString tranStringPrivate(const QString& src) override;
    void doneTranPrivate(bool lazyClose) override;
    bool isReady() override;

};

#endif // WEBAPIABSTRACTTRANSLATOR_H
