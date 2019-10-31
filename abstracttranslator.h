#ifndef CABSTRACTTRANSLATOR_H
#define CABSTRACTTRANSLATOR_H

#include <QObject>
#include <QString>
#include "globalcontrol.h"
#include "structures.h"

namespace CDefaults {
const int translatorConnectionTimeout = 60000;
}

class CAbstractTranslator : public QObject
{
    Q_OBJECT
private:
    QString m_tranError;
    CLangPair m_lang;

    Q_DISABLE_COPY(CAbstractTranslator)

protected:
    void setErrorMsg(const QString& msg);
    void clearErrorMsg();
    CLangPair language() const;
    void setLanguage(const CLangPair& lang);
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

    static CAbstractTranslator* translatorFactory(QObject *parent, const CLangPair &tranDirection);

};

#endif // CABSTRACTTRANSLATOR_H
