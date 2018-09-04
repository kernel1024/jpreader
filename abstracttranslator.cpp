#include "abstracttranslator.h"
#include "atlastranslator.h"
#include "bingtranslator.h"
#include "yandextranslator.h"
#include "googlegtxtranslator.h"

CAbstractTranslator::CAbstractTranslator(QObject *parent, const CLangPair &lang) : QObject(parent)
{
    tranError.clear();
    m_lang = lang;
}

CAbstractTranslator::~CAbstractTranslator()
{

}

QString CAbstractTranslator::getErrorMsg()
{
    return tranError;
}

CAbstractTranslator *translatorFactory(QObject* parent, const CLangPair& tranDirection)
{
    if (!tranDirection.isValid()) return nullptr;

    if (gSet->settings.translatorEngine==TE_ATLAS) {
        return new CAtlasTranslator(parent, gSet->settings.atlHost, gSet->settings.atlPort, tranDirection);
    } else if (gSet->settings.translatorEngine==TE_BINGAPI)
        return new CBingTranslator(parent, tranDirection, gSet->settings.bingKey);
    else if (gSet->settings.translatorEngine==TE_YANDEX)
        return new CYandexTranslator(parent, tranDirection, gSet->settings.yandexKey);
    else if (gSet->settings.translatorEngine==TE_GOOGLE_GTX)
        return new CGoogleGTXTranslator(parent, tranDirection);
    else
        return nullptr;
}
