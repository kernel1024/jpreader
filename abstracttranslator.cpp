#include "abstracttranslator.h"
#include "globalcontrol.h"
#include "atlastranslator.h"
#include "bingtranslator.h"
#include "yandextranslator.h"

CAbstractTranslator::CAbstractTranslator(QObject *parent) : QObject(parent)
{
    tranError.clear();
}

CAbstractTranslator::~CAbstractTranslator()
{

}

QString CAbstractTranslator::getErrorMsg()
{
    return tranError;
}

CAbstractTranslator *translatorFactory(QObject* parent)
{
    if (gSet->settings.translatorEngine==TE_ATLAS)
        return new CAtlasTranslator(parent,gSet->settings.atlHost,gSet->settings.atlPort);
    else if (gSet->settings.translatorEngine==TE_BINGAPI)
        return new CBingTranslator(parent,gSet->settings.bingID,gSet->settings.bingKey);
    else if (gSet->settings.translatorEngine==TE_YANDEX)
        return new CYandexTranslator(parent,gSet->settings.yandexKey);
    else
        return NULL;
}
