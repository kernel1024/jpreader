#include "deeplfreetranslator.h"
#include "global/control.h"
#include "global/browserfuncs.h"

namespace CDefaults {
const int deeplMaxFailedQueriesInRow = 4;
}

CDeeplFreeTranslator::CDeeplFreeTranslator(QObject *parent, const CLangPair &lang)
    : CAbstractTranslator(parent,lang)
{
}

CDeeplFreeTranslator::~CDeeplFreeTranslator() = default;

bool CDeeplFreeTranslator::initTran()
{
    m_failedQueriesCounter = 0;
    return true;
}

QString CDeeplFreeTranslator::tranStringPrivate(const QString &src)
{
    if (!isReady()) {
        setErrorMsg(tr("ERROR: Translator not ready"));
        return QSL("ERROR:TRAN_NOT_READY");
    }

    QString res;
    for (const QString &str : splitLongText(src)) {
        QString s = tranStringInternal(str);
        if (!getErrorMsg().isEmpty()) {
            res=s;
            break;
        }
        res.append(s);
    }
    return res;
}

QString CDeeplFreeTranslator::tranStringInternal(const QString &src)
{
    if (src.isEmpty())
        return QString();

    QUrl url(QSL("https://www.deepl.com/translator#%1/%2/%3")
             .arg(language().langFrom.bcp47Name(),
                  language().langTo.bcp47Name(),
                  QUrl::toPercentEncoding(src)));

    QString res;
    for (int retry = 0; retry < gSet->settings()->translatorRetryCount; retry++) {
        gSet->browser()->headlessDOMWorker(url,
                                           QSL("document.querySelector('textarea[dl-test="
                                               "translator-target-input]').value;"),
                                           [&res](const QVariant &result) -> bool {
            const QString str = result.toString();
            if (!str.isEmpty())
                res = str;
            return !str.isEmpty();
        });

        if (!res.isEmpty())
            break;

        qWarning() << QSL("DeepL request retry #%1").arg(retry+1);
    }


    if (res.isEmpty()) {
        if (m_failedQueriesCounter < CDefaults::deeplMaxFailedQueriesInRow) {
            res = src;
        } else {
            res = QSL("ERROR:DEEPL_NO_RESPONSE");
        }
        m_failedQueriesCounter++;
    } else {
        m_failedQueriesCounter = 0;
    }

    if (res.startsWith(QSL("ERROR:")))
        setErrorMsg(tr("ERROR: DeepL translator error or timeout."));

    return res;
}

void CDeeplFreeTranslator::doneTranPrivate(bool lazyClose)
{
    Q_UNUSED(lazyClose)
}

bool CDeeplFreeTranslator::isReady()
{
    return gSet->browser()->isDOMWorkerReady();
}

CStructures::TranslationEngine CDeeplFreeTranslator::engine()
{
    return CStructures::teDeeplFree;
}
