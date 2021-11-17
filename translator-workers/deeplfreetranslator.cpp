#include "deeplfreetranslator.h"
#include "global/control.h"
#include "global/browserfuncs.h"

CDeeplFreeTranslator::CDeeplFreeTranslator(QObject *parent, const CLangPair &lang)
    : CAbstractTranslator(parent,lang)
{
}

CDeeplFreeTranslator::~CDeeplFreeTranslator() = default;

bool CDeeplFreeTranslator::initTran()
{
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
    QUrl url(QSL("https://www.deepl.com/translator#%1/%2/%3")
             .arg(language().langFrom.bcp47Name(),
                  language().langTo.bcp47Name(),
                  QUrl::toPercentEncoding(src)));

    QString res;
    gSet->browser()->headlessDOMWorker(url,
                                       QSL("document.querySelector('textarea[dl-test=translator-target-input]').value;"),
                                       [&res](const QVariant &result) -> bool {
        const QString str = result.toString();
        if (!str.isEmpty())
            res = str;
        return !str.isEmpty();
    });

    if (res.isEmpty())
        res = QSL("ERROR:DEEPL_NO_RESPONSE");

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
