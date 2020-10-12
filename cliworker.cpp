#include <QCommandLineParser>
#include <QThread>
#include <QDebug>
#include "cliworker.h"
#include "structures.h"
#include "genericfuncs.h"
#include "translators/abstracttranslator.h"
#include "translators/atlastranslator.h"

namespace CDefaults {
const int cliHelpColumnWidth = -16;
}

CCLIWorker::CCLIWorker(QObject *parent)
    : QObject(parent),
      m_in(new QTextStream(stdin)),
      m_out(new QTextStream(stdout)),
      m_err(new QTextStream(stderr))
{
}

CCLIWorker::~CCLIWorker()
{
    m_out->flush();
    m_err->flush();

    delete m_in;
    delete m_out;
    delete m_err;
}

bool CCLIWorker::parseArguments()
{
    QCommandLineParser parser;
    parser.setApplicationDescription(tr("Special enhanced browser for translating, searching and reading assistance."));
    const auto optHelp = parser.addHelpOption();
    const auto optVersion = parser.addVersionOption();
    const QCommandLineOption optSource({ QSL("f"), QSL("from") },
                                       tr("Source language code (two letter ISO 639 code: en, ja, zh...)."),
                                       QSL("from"), QSL("ja") );
    const QCommandLineOption optDestination({ QSL("t"), QSL("to") },
                                            tr("Destination language code."),
                                            QSL("to"), QSL("en") );
    const QCommandLineOption optEngine({ QSL("e"), QSL("engine") },
                                       tr("Translation engine (use --list-engines for valid values)."),
                                       QSL("engine"), QSL("atlas") );

    const QCommandLineOption optListEngines({ QSL("E"), QSL("list-engines") }, tr("List available translation engines."));
    const QCommandLineOption optListBCP47Codes({ QSL("L"), QSL("list-languages") }, tr("List available language codes."));
    const QCommandLineOption optSubsentences({ QSL("s"), QSL("subsentences") }, tr("Enable subsentence mode."));

    parser.addOptions({ optSource, optDestination, optEngine, optListEngines, optListBCP47Codes, optSubsentences });
    parser.addPositionalArgument(QSL("text"),
                                 tr("Text to translate."),
                                 QSL("[text]") );

    parser.process(QCoreApplication::arguments());

    if (parser.isSet(optListEngines)) {
        *m_out << tr("Valid engine codes:") << Qt::endl;
        for (auto it = CStructures::translationEngineCodes().constBegin(),
             end = CStructures::translationEngineCodes().constEnd(); it != end; ++it) {
            const QString desc = CStructures::translationEngines().value(it.key());
            *m_out << QSL("    %1%2").arg(it.value(),CDefaults::cliHelpColumnWidth).arg(desc) << Qt::endl;
        }

        return false;
    }

    if (parser.isSet(optListBCP47Codes)) {
        *m_out << tr("Valid language codes:") << Qt::endl;
        const QStringList languages = gSet->getLanguageCodes();
        for (const auto &bcp : languages) {
            *m_out << QSL("    %1%2").arg(bcp,CDefaults::cliHelpColumnWidth).arg(gSet->getLanguageName(bcp)) << Qt::endl;
        }

        return false;
    }

    m_lang = CLangPair(parser.value(optSource),parser.value(optDestination));
    if (!m_lang.isValid()) {
        *m_out << tr("Unacceptable language pair: %1").arg(m_lang.toString()) << Qt::endl;
        return false;
    }

    const QString engine = parser.value(optEngine);
    m_engine = CAbstractTranslator::translatorFactory(nullptr,m_lang,engine);
    if (m_engine.isNull()) {
        *m_out << tr("Unable to create translator %1 for language pair: %2")
                  .arg(engine,m_lang.toString()) << Qt::endl;
        return false;
    }

    m_subsentences = parser.isSet(optSubsentences);
    m_preloadedText = parser.positionalArguments();

    m_error.clear();

    return true;
}

void CCLIWorker::start()
{
    if (m_preloadedText.isEmpty()) {
        m_in->skipWhiteSpace();
        while (!m_in->atEnd())
            translateStrings({ m_in->readLine() });

    } else {
        translateStrings(m_preloadedText);
    }
    Q_EMIT finished();
}

void CCLIWorker::translateStrings(const QStringList &list)
{
    if (m_engine.isNull()) return;
    m_error.clear();
    m_engine->initTran();

    for (const auto &s : list) {
        QString res = translate(s);
        if (!m_error.isEmpty()) {
            *m_err << tr("Translation error. ") << m_error << Qt::endl;
            break;
        }
        *m_out << res << Qt::endl;
    }
}

QString CCLIWorker::translate(const QString &text)
{
    QString res;
    if (m_engine.isNull()) return res;

    if (m_engine->engine()==CStructures::teAtlas) {

        if (!m_lang.isAtlasAcceptable()) {
            m_error = tr("ATLAS error: Unacceptable translation pair. Only English and Japanese is supported.");
        } else {
            for (int i=0;i<gSet->settings()->translatorRetryCount;i++) {
                if (!m_engine->initTran()) {
                    m_error = tr("ATLAS error: Unable to initialize engine.");
                    return res;
                }

                m_error.clear();
                res = translatePriv(text);

                if (m_error.isEmpty())
                    break;

                QThread::msleep(m_engine->getRandomDelay(CDefaults::atlasMinRetryDelay,
                                                         CDefaults::atlasMaxRetryDelay));
            }
        }

    } else {
        res = translatePriv(text);
    }

    return res;
}

QString CCLIWorker::translatePriv(const QString &text)
{
    QString sout;
    QStringList srctls;

    const QChar questionMark('?');
    const QChar fullwidthQuestionMark(0xff1f);

    QString srct = text;
    srct = srct.replace('\r',' ');
    srct = srct.replace('\n',' ');

    if (!srct.trimmed().isEmpty()) {
        QString t;

        QString ttest = srct;
        bool noText = true;
        ttest.remove(QRegularExpression(QSL("&\\w+;")));
        for (const QChar &tc : qAsConst(ttest)) {
            if (tc.isLetter()) {
                noText = false;
                break;
            }
        }

        if (!noText) {
            if (m_subsentences) {
                QString tacc = QString();
                for (int j=0;j<srct.length();j++) {
                    QChar sc = srct.at(j);

                    if (sc.isLetterOrNumber())
                        tacc += sc;
                    if (!sc.isLetterOrNumber() || (j==(srct.length()-1))) {
                        if (!tacc.isEmpty()) {
                            if (sc.isLetterOrNumber()) {
                                t += m_engine->tranString(tacc);
                            } else {
                                if (sc==questionMark || sc==fullwidthQuestionMark) {
                                    tacc += sc;
                                    t += m_engine->tranString(tacc);
                                } else {
                                    t += m_engine->tranString(tacc) + sc;
                                }
                            }
                            tacc.clear();
                        } else {
                            t += sc;
                        }
                    }
                    if (!m_engine->getErrorMsg().isEmpty())
                        break;
                }
            } else {
                t = m_engine->tranString(srct);
            }
        } else {
            t = srct;
        }

        if (!m_engine->getErrorMsg().isEmpty()) {
            m_error = m_engine->getErrorMsg();
            m_engine->doneTran();
        }

        sout = t;
    }

    return sout;
}
