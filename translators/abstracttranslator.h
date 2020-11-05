#ifndef CABSTRACTTRANSLATOR_H
#define CABSTRACTTRANSLATOR_H

#include <QObject>
#include <QString>
#include "../globalcontrol.h"
#include "../structures.h"

namespace CDefaults {
const int translatorConnectionTimeout = 10000;
const int tranMinRetryDelay = 10000;
const int tranMaxRetryDelay = 25000;
const int tranAliDelayFrac = 10;
}

class CAbstractTranslator : public QObject
{
    Q_OBJECT
private:
    QString m_tranError;
    CLangPair m_lang;
    int m_translatorRetryCount { 10 };
    qint64 m_requestTotalSize { 0L };
    qint64 m_requestCount { 0L };

    Q_DISABLE_COPY(CAbstractTranslator)

protected:
    void setErrorMsg(const QString& msg);
    void clearErrorMsg();
    CLangPair language() const;
    void setLanguage(const CLangPair& lang);
    bool isAborted();
    void addLoadedRequest(qint64 size);

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

    static CAbstractTranslator* translatorFactory(QObject *parent, const CLangPair &tranDirection,
                                                  const QString &engineName = QString());

Q_SIGNALS:
    void dataLoaded();

};

#endif // CABSTRACTTRANSLATOR_H
