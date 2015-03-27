#ifndef CABSTRACTTRANSLATOR_H
#define CABSTRACTTRANSLATOR_H

#include <QObject>
#include <QString>

class CAbstractTranslator : public QObject
{
    Q_OBJECT
public:
    explicit CAbstractTranslator(QObject *parent = 0);
    ~CAbstractTranslator();

    virtual bool initTran()=0;
    virtual QString tranString(QString src)=0;
    virtual void doneTran(bool lazyClose = false)=0;
    virtual bool isReady()=0;

signals:

public slots:
};

CAbstractTranslator* translatorFactory(QObject *parent);

#endif // CABSTRACTTRANSLATOR_H
