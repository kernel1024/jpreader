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

QString CAbstractTranslator::getErrorMsg()
{
    return tranError;
}

CAbstractTranslator *translatorFactory(QObject* parent, const CLangPair& tranDirection)
{
    if (!tranDirection.isValid()) return nullptr;

    if (gSet->settings.translatorEngine==TE_ATLAS)
        return new CAtlasTranslator(parent, gSet->settings.atlHost, gSet->settings.atlPort, tranDirection);

    if (gSet->settings.translatorEngine==TE_BINGAPI)
        return new CBingTranslator(parent, tranDirection, gSet->settings.bingKey);

    if (gSet->settings.translatorEngine==TE_YANDEX)
        return new CYandexTranslator(parent, tranDirection, gSet->settings.yandexKey);

    if (gSet->settings.translatorEngine==TE_GOOGLE_GTX)
        return new CGoogleGTXTranslator(parent, tranDirection);

    return nullptr;
}
