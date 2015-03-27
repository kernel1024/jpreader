#include "abstracttranslator.h"
#include "globalcontrol.h"
#include "atlastranslator.h"

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
    else
        return NULL;
}
