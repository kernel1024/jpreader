#include <utility>

#include <QThread>
#include <QScopedPointer>
#include "auxtranslator.h"
#include "utils/specwidgets.h"
#include "global/control.h"

CAuxTranslator::CAuxTranslator(QObject *parent) :
    QObject(parent)
{
    m_text = QString();
    m_lang = CLangPair(QSL("ja"),QSL("en"));
}

void CAuxTranslator::setText(const QString &text)
{
    m_text = text;
}

void CAuxTranslator::setSrcLang(const QString &lang)
{
    m_lang.langFrom = QLocale(lang);
}

void CAuxTranslator::setDestLang(const QString &lang)
{
    m_lang.langTo = QLocale(lang);
}

QString CAuxTranslator::translateText(const QString &text)
{
    if (text.isEmpty())
        return text;

    QScopedPointer<CAbstractTranslator, QScopedPointerDeleteLater> tran(
            CAbstractTranslator::translatorFactory(
                this,
                gSet->settings()->translatorEngine,
                m_lang
                )
            );

    if (!tran || !tran->initTran()) {
        qCritical()
        << tr("Unable to initialize translation engine.");

        return QSL("ERROR");
    }

    QString source = text;

    source.replace(QSL("\r\n"), QSL("\n"));
    source.replace(u'\r', u'\n');

    const QStringList lines =
        source.split(u'\n', Qt::KeepEmptyParts);

    QString result;

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            result.append(u'\n');
            continue;
        }

        const QString translated =
            tran->tranString(line);

        if (translated.trimmed().isEmpty())
            result.append(line);
        else
            result.append(translated);

        result.append(u'\n');
    }

    tran->doneTran();

    return result;
}

void CAuxTranslator::translatePriv()
{
    m_text = translateText(m_text);

    Q_EMIT gotTranslation(m_text);
}

void CAuxTranslator::translateAndQuit()
{
    translatePriv();
    QThread::currentThread()->quit();
}

void CAuxTranslator::startAuxTranslation(const QString &text)
{
    setText(text);
    translatePriv();
}

void CAuxTranslator::startTranslationRequest(qulonglong requestId, const QString &text)
{
    const QString result = translateText(text);

    Q_EMIT translationRequestFinished(
        requestId,
        result
        );
}
