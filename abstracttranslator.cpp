#include "abstracttranslator.h"
#include "atlastranslator.h"
#include "bingtranslator.h"
#include "yandextranslator.h"
#include "googlegtxtranslator.h"

void CAbstractTranslator::setErrorMsg(const QString &msg)
{
    m_tranError = msg;
}

void CAbstractTranslator::clearErrorMsg()
{
    m_tranError.clear();
}

CLangPair CAbstractTranslator::language() const
{
    return m_lang;
}

void CAbstractTranslator::setLanguage(const CLangPair &lang)
{
    m_lang = lang;
}

CAbstractTranslator::CAbstractTranslator(QObject *parent, const CLangPair &lang) : QObject(parent)
{
    m_tranError.clear();
    m_lang = lang;
}

QString CAbstractTranslator::getErrorMsg() const
{
    return m_tranError;
}

CAbstractTranslator *translatorFactory(QObject* parent, const CLangPair& tranDirection)
{
    if (!tranDirection.isValid()) return nullptr;

    if (gSet->settings.translatorEngine==teAtlas)
        return new CAtlasTranslator(parent, gSet->settings.atlHost, gSet->settings.atlPort, tranDirection);

    if (gSet->settings.translatorEngine==teBingAPI)
        return new CBingTranslator(parent, tranDirection, gSet->settings.bingKey);

    if (gSet->settings.translatorEngine==teYandexAPI)
        return new CYandexTranslator(parent, tranDirection, gSet->settings.yandexKey);

    if (gSet->settings.translatorEngine==teGoogleGTX)
        return new CGoogleGTXTranslator(parent, tranDirection);

    return nullptr;
}
