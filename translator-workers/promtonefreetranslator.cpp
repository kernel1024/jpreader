#include <QUrlQuery>
#include "promtonefreetranslator.h"
#include "global/control.h"
#include "global/browserfuncs.h"

namespace CDefaults {
const int promtMaxFailedQueriesInRow = 10;
}

CPromtOneFreeTranslator::CPromtOneFreeTranslator(QObject *parent, const CLangPair &lang)
    : CAbstractTranslator(parent,lang)
{
}

CPromtOneFreeTranslator::~CPromtOneFreeTranslator() = default;

bool CPromtOneFreeTranslator::initTran()
{
    m_failedQueriesCounter = 0;
    return true;
}

QString CPromtOneFreeTranslator::tranStringPrivate(const QString &src)
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

QString CPromtOneFreeTranslator::tranStringInternal(const QString &src)
{
    if (src.isEmpty())
        return QString();

    const QString srcLang = promtLangCode(language().langFrom.bcp47Name());
    const QString dstLang = promtLangCode(language().langTo.bcp47Name());

    QString res;
    if (srcLang.isEmpty() || dstLang.isEmpty()) {
        res = QSL("ERROR:PROMT_UNSUPPORTED_LANGUAGE");
        setErrorMsg(tr("ERROR: PROMT One translator error - unsupported language."));
        return res;
    }

    QUrl url(QSL("https://translate.ru/перевод/%1-%2").arg(srcLang,dstLang));
    QUrlQuery qr;
    qr.addQueryItem(QSL("text"),src);
    url.setQuery(qr);

    for (int retry = 0; retry < gSet->settings()->translatorRetryCount; retry++) {
        gSet->browser()->headlessDOMWorker(url,
                                           QSL("document.querySelector('textarea[id="
                                               "tText]').value;"),
                                           [&res](const QVariant &result) -> bool {
            QString str = result.toString();
            if (str == QSL("Что-то пошло не так :-("))
                str.clear();
            if (!str.isEmpty())
                res = str;
            return !str.isEmpty();
        });

        if (!res.isEmpty())
            break;

        qWarning() << QSL("PROMT One request retry #%1").arg(retry+1);
    }


    if (res.isEmpty()) {
        if (m_failedQueriesCounter < CDefaults::promtMaxFailedQueriesInRow) {
            res = src;
        } else {
            res = QSL("ERROR:PROMT_NO_RESPONSE");
        }
        m_failedQueriesCounter++;
    } else {
        m_failedQueriesCounter = 0;
    }

    if (res.startsWith(QSL("ERROR:")))
        setErrorMsg(tr("ERROR: PROMT One translator error or timeout."));

    return res;
}

QString CPromtOneFreeTranslator::promtLangCode(const QString &bcp47name) const
{
    static const QHash<QString,QString>
            promtLanguages({
                               { QSL("az") , QSL("азербайджанский") },
                               { QSL("en") , QSL("английский") },
                               { QSL("ar") , QSL("арабский") },
                               { QSL("el") , QSL("греческий") },
                               { QSL("he") , QSL("иврит") },
                               { QSL("es") , QSL("испанский") },
                               { QSL("it") , QSL("итальянский") },
                               { QSL("kk") , QSL("казахский") },
                               { QSL("zh") , QSL("китайский") },
                               { QSL("ko") , QSL("корейский") },
                               { QSL("de") , QSL("немецкий") },
                               { QSL("pt") , QSL("португальский") },
                               { QSL("ru") , QSL("русский") },
                               { QSL("tt") , QSL("татарский") },
                               { QSL("tr") , QSL("турецкий") },
                               { QSL("tk") , QSL("туркменский") },
                               { QSL("uz") , QSL("узбекский") },
                               { QSL("uk") , QSL("украинский") },
                               { QSL("fi") , QSL("финский") },
                               { QSL("fr") , QSL("французский") },
                               { QSL("et") , QSL("эстонский") },
                               { QSL("ja") , QSL("японский") }
                           });

    return promtLanguages.value(bcp47name,QString());
}

void CPromtOneFreeTranslator::doneTranPrivate(bool lazyClose)
{
    Q_UNUSED(lazyClose)
}

bool CPromtOneFreeTranslator::isReady()
{
    return gSet->browser()->isDOMWorkerReady();
}

CStructures::TranslationEngine CPromtOneFreeTranslator::engine()
{
    return CStructures::tePromtOneFree;
}
