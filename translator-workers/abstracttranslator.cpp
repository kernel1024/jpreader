#include <QRandomGenerator>
#include <QRegularExpression>
#include "abstracttranslator.h"
#include "atlastranslator.h"
#include "bingtranslator.h"
#include "yandextranslator.h"
#include "googlegtxtranslator.h"
#include "awstranslator.h"
#include "yandexcloudtranslator.h"
#include "googlecloudtranslator.h"
#include "alicloudtranslator.h"
#include "deeplfreetranslator.h"
#include "promtonefreetranslator.h"
#include "promtnmttranslator.h"
#include "deeplapitranslator.h"
#include "openaitranslator.h"
#include "translator/translator.h"
#include "global/control.h"
#include "global/network.h"

namespace CDefaults {
const int maxTranslationStringLength = 5000;
}

int CAbstractTranslator::getTranslatorRetryCount() const
{
    return m_translatorRetryCount;
}

bool CAbstractTranslator::isAborted()
{
    auto *sequencedTranslator = qobject_cast<CTranslator *>(parent());

    if (sequencedTranslator==nullptr) // singleshot translator - no abortion
        return false;

    return sequencedTranslator->isAborted();
}

QStringList CAbstractTranslator::splitLongText(const QString &src) const
{
    QStringList res;
    if (src.length()>=CDefaults::maxTranslationStringLength) {
        const int margin = 10;

        // Split by separator chars
        // RegEx for ' ' or ',' or '.' or ':' or '\t'
        static const QRegularExpression rx(QSL("(\\ |\\,|\\.|\\:|\\t)"));
        QStringList srcl = src.split(rx);
        // Check for max length
        for (int i=0;i<srcl.count();i++) {
            QString s = srcl.at(i);
            while (!s.isEmpty()) {
                res.append(s.left(CDefaults::maxTranslationStringLength - margin));
                s.remove(0,CDefaults::maxTranslationStringLength - margin);
            }
        }
    } else {
        res.append(src);
    }
    return res;
}

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

CAbstractTranslator::CAbstractTranslator(QObject *parent, const CLangPair &lang)
    : QObject(parent),
      m_lang(lang),
      m_translatorRetryCount(gSet->settings()->translatorRetryCount)
{
}

void CAbstractTranslator::doneTran(bool lazyClose)
{
    doneTranPrivate(lazyClose);
    const CStructures::TranslationEngine eng = engine();
    QMetaObject::invokeMethod(gSet,[eng](){
        gSet->net()->addTranslatorStatistics(eng, -1); // Force statistics update signal
    },Qt::QueuedConnection);
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
    QMetaObject::invokeMethod(gSet,[eng,len](){
        gSet->net()->addTranslatorStatistics(eng, len);
    },Qt::QueuedConnection);
    return tranStringPrivate(src);
}

unsigned long CAbstractTranslator::getRandomDelay(int min, int max)
{
    int m_min = min;
    if (m_min<1) m_min = 1;
    int m_max = max;
    if (m_max<2) m_max = 2;
    return static_cast<unsigned long>(QRandomGenerator::global()->bounded(m_min,m_max));
}

CAbstractTranslator* CAbstractTranslator::translatorFactory(QObject* parent,
                                                            CStructures::TranslationEngine engine,
                                                            const CLangPair& tranDirection)
{
    CAbstractTranslator* res = nullptr;

    if (!tranDirection.isValid()) return res;

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
    if (engine==CStructures::teYandexCloud) {
        res = new CYandexCloudTranslator(parent, tranDirection, gSet->settings()->yandexCloudApiKey,
                                         gSet->settings()->yandexCloudFolderID);
    }
    if (engine==CStructures::teGoogleCloud) {
        res = new CGoogleCloudTranslator(parent, tranDirection, gSet->settings()->gcpJsonKeyFile);
    }
    if (engine==CStructures::teAliCloud) {
        res = new CAliCloudTranslator(parent, tranDirection, gSet->settings()->aliCloudTranslatorMode,
                                      gSet->settings()->aliAccessKeyID, gSet->settings()->aliAccessKeySecret);
    }
    if (engine==CStructures::teDeeplFree) {
        res = new CDeeplFreeTranslator(parent, tranDirection);
    }
    if (engine==CStructures::tePromtOneFree) {
        res = new CPromtOneFreeTranslator(parent, tranDirection);
    }
    if (engine==CStructures::tePromtNmtAPI) {
        res = new CPromtNmtTranslator(parent, tranDirection, gSet->settings()->promtNmtServer,
                                      gSet->settings()->promtNmtAPIKey);
    }
    if (engine==CStructures::teDeeplAPI) {
        res = new CDeeplAPITranslator(parent, tranDirection, gSet->settings()->deeplAPIMode,
                                      gSet->settings()->deeplAPIKey,gSet->settings()->deeplAPISplitSentences,
                                      gSet->settings()->deeplAPIFormality);
    }
    if (engine==CStructures::teOpenAI) {
        res = new COpenAITranslator(parent, tranDirection, gSet->settings()->openaiTranslationModel,
                                    gSet->settings()->openaiAPIKey, gSet->settings()->openaiTemperature,
                                    gSet->settings()->openaiTopP, gSet->settings()->openaiPresencePenalty,
                                    gSet->settings()->openaiFrequencyPenalty);
    }

    return res;
}
