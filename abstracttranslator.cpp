#include "abstracttranslator.h"
#include "globalcontrol.h"
#include "atlastranslator.h"
#include "bingtranslator.h"
#include "yandextranslator.h"

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
    else if (gSet->translatorEngine==TE_BINGAPI)
        return new CBingTranslator(parent,gSet->bingID,gSet->bingKey);
    else if (gSet->translatorEngine==TE_YANDEX)
        return new CYandexTranslator(parent,gSet->yandexKey);
    else
        return NULL;
}
