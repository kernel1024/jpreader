#include "auxtranslator.h"
#include "specwidgets.h"
#include "globalcontrol.h"

CAuxTranslator::CAuxTranslator(QObject *parent) :
    QObject(parent)
{
    text = QString();
}

void CAuxTranslator::setParams(const QString &Text)
{
    text = Text;
}

void CAuxTranslator::startTranslation(bool deleteAfter)
{
    if (!text.isEmpty()) {
        CAbstractTranslator* tran=translatorFactory(this);
        if (tran==NULL || !tran->initTran()) {
            qCritical() << tr("Unable to initialize translation engine.");
            text = "ERROR";
        } else {
            text = tran->tranString(text);
            tran->doneTran();
        }
        tran->deleteLater();
    }
    emit gotTranslation(text);
    if (deleteAfter)
        deleteLater();
}

void CAuxTranslator::startAuxTranslation(const QString &text)
{
    setParams(text);
    startTranslation(false);
}

