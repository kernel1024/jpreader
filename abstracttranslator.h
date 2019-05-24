#ifndef CABSTRACTTRANSLATOR_H
#define CABSTRACTTRANSLATOR_H

#include <QObject>
#include <QString>
#include "globalcontrol.h"

class CAbstractTranslator : public QObject
{
    Q_OBJECT
private:
    QString m_translatorError;
    CLangPair m_langPair;

protected:
    void setTranslatorError(const QString& msg);
    void clearTranslatorError();
    CLangPair getLangPair() const;
    void setLangPair(const CLangPair &lang);

public:
    explicit CAbstractTranslator(QObject *parent, const CLangPair& lang);

    virtual bool initTran()=0;
    virtual QString tranString(const QString& src)=0;
    virtual void doneTran(bool lazyClose = false)=0;
    virtual bool isReady()=0;
    QString getErrorMsg();

signals:

public slots:
};

CAbstractTranslator* translatorFactory(QObject *parent, const CLangPair &tranDirection);

#endif // CABSTRACTTRANSLATOR_H
