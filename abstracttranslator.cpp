#include "abstracttranslator.h"
#include "atlastranslator.h"
#include "bingtranslator.h"
#include "yandextranslator.h"
#include "googlegtxtranslator.h"
#include "awstranslator.h"

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

void CAbstractTranslator::doneTran(bool lazyClose)
{
    doneTranPrivate(lazyClose);
    const CStructures::TranslationEngine eng = engine();
    QTimer::singleShot(0,gSet,[eng](){
        gSet->addTranslatorStatistics(eng, -1); // Force statistics update signal
    });
}

QString CAbstractTranslator::getErrorMsg() const
{
    return m_tranError;
}

QString CAbstractTranslator::tranString(const QString &src)
{
    if (src.isEmpty())
        return src;

    const int len = src.length();
    const CStructures::TranslationEngine eng = engine();
    QTimer::singleShot(0,gSet,[eng,len](){
        gSet->addTranslatorStatistics(eng, len);
    });
    return tranStringPrivate(src);
}

CAbstractTranslator* CAbstractTranslator::translatorFactory(QObject* parent, const CLangPair& tranDirection)
{
    CAbstractTranslator* res = nullptr;

    if (!tranDirection.isValid()) return res;

    CStructures::TranslationEngine engine = gSet->settings()->translatorEngine;

    if (engine==CStructures::teAtlas)
        res = new CAtlasTranslator(parent, gSet->settings()->atlHost, gSet->settings()->atlPort, tranDirection);

    if (engine==CStructures::teBingAPI)
        res = new CBingTranslator(parent, tranDirection, gSet->settings()->bingKey);

    if (engine==CStructures::teYandexAPI)
        res = new CYandexTranslator(parent, tranDirection, gSet->settings()->yandexKey);

    if (engine==CStructures::teGoogleGTX)
        res = new CGoogleGTXTranslator(parent, tranDirection);

    if (engine==CStructures::teAmazonAWS) {
        res = new CAWSTranslator(parent, tranDirection, gSet->settings()->awsRegion,
                                  gSet->settings()->awsAccessKey, gSet->settings()->awsSecretKey);
    }

    return res;
}
