#ifndef CABSTRACTTRANSLATOR_H
#define CABSTRACTTRANSLATOR_H

#include <QObject>
#include <QString>
#include "../globalcontrol.h"
#include "../structures.h"

namespace CDefaults {
const int translatorConnectionTimeout = 10000;
const unsigned int tranMinRetryDelay = 10;
const unsigned int tranMaxRetryDelay = 25;
}

class CAbstractTranslator : public QObject
{
    Q_OBJECT
private:
    QString m_tranError;
    CLangPair m_lang;
    int m_translatorRetryCount { 10 };

    Q_DISABLE_COPY(CAbstractTranslator)

protected:
    void setErrorMsg(const QString& msg);
    void clearErrorMsg();
    CLangPair language() const;
    void setLanguage(const CLangPair& lang);
    bool isAborted();

public:
    explicit CAbstractTranslator(QObject *parent, const CLangPair& lang);
    ~CAbstractTranslator() override = default;

    virtual bool initTran()=0;
    virtual QString tranStringPrivate(const QString& src)=0;
    virtual void doneTranPrivate(bool lazyClose)=0;
    virtual bool isReady()=0;
    virtual CStructures::TranslationEngine engine()=0;

    void doneTran(bool lazyClose = false);
    QString getErrorMsg() const;
    QString tranString(const QString& src);
    unsigned long getRandomDelay(int min = CDefaults::tranMinRetryDelay, int max = CDefaults::tranMaxRetryDelay);
    int getTranslatorRetryCount() const;

    static CAbstractTranslator* translatorFactory(QObject *parent, const CLangPair &tranDirection);

};

#endif // CABSTRACTTRANSLATOR_H
