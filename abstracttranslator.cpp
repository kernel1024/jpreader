#include "abstracttranslator.h"
#include "atlastranslator.h"
#include "bingtranslator.h"
#include "yandextranslator.h"
#include "googlegtxtranslator.h"

CAbstractTranslator::CAbstractTranslator(QObject *parent, const QString &SrcLang) : QObject(parent)
{
    tranError.clear();
    srcLang = SrcLang;
}

CAbstractTranslator::~CAbstractTranslator()
{

}

QString CAbstractTranslator::getErrorMsg()
{
    return tranError;
}

CAbstractTranslator *translatorFactory(QObject* parent, int tranDirection)
{
    if (gSet->settings.translatorEngine==TE_ATLAS) {
        QString dir;
        switch (tranDirection) {
            case LS_AUTO: dir = QString("auto"); break;
            case LS_ENGLISH: dir = QString("en"); break;
            case LS_JAPANESE: dir = QString("ja"); break;
            default: dir = gSet->getSourceLanguageID(TE_ATLAS); break; // also LS_GLOBAL
        }
        return new CAtlasTranslator(parent,gSet->settings.atlHost,gSet->settings.atlPort, dir);
    } else if (gSet->settings.translatorEngine==TE_BINGAPI)
        return new CBingTranslator(parent,gSet->getSourceLanguageID(TE_BINGAPI),
                                   gSet->settings.bingID,gSet->settings.bingKey);
    else if (gSet->settings.translatorEngine==TE_YANDEX)
        return new CYandexTranslator(parent,gSet->getSourceLanguageID(TE_YANDEX),
                                     gSet->settings.yandexKey);
    else if (gSet->settings.translatorEngine==TE_GOOGLE_GTX)
        return new CGoogleGTXTranslator(parent,gSet->getSourceLanguageID(TE_GOOGLE_GTX));
    else
        return NULL;
}
