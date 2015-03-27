#include "abstracttranslator.h"
#include "globalcontrol.h"
#include "atlastranslator.h"
#include "bingtranslator.h"

CAbstractTranslator::CAbstractTranslator(QObject *parent) : QObject(parent)
{

}

CAbstractTranslator::~CAbstractTranslator()
{

}

CAbstractTranslator *translatorFactory(QObject* parent)
{
    if (gSet->translatorEngine==TE_ATLAS)
        return new CAtlasTranslator(parent,gSet->atlHost,gSet->atlPort);
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    else if (gSet->translatorEngine==TE_BINGAPI)
        return new CBingTranslator(parent,gSet->bingID,gSet->bingKey);
#endif
    else
        return NULL;
}
