#include "auxtranslator.h"
#include "globalcontrol.h"

CAuxTranslator::CAuxTranslator(QObject *parent) :
    QObject(parent)
{
    tranMode = CAtlasTranslator::AutoTran;
    text = QString();
}

void CAuxTranslator::setParams(const QString &Text, const CAtlasTranslator::ATTranslateMode TranMode)
{
    tranMode = TranMode;
    text = Text;
}

void CAuxTranslator::startTranslation(bool deleteAfter)
{
    if (!text.isEmpty()) {
        CAtlasTranslator atlas;
        if (!atlas.initTran(gSet->atlHost,gSet->atlPort)) {
            qDebug() << tr("Unable to initialize ATLAS.");
            text = "ERROR";
        } else {
            text = atlas.tranString(text);
            atlas.doneTran();
        }
    }
    emit gotTranslation(text);
    if (deleteAfter)
        deleteLater();
}

void CAuxTranslator::startAuxTranslation(const QString &text)
{
    setParams(text, CAtlasTranslator::AutoTran);
    startTranslation(false);
}
