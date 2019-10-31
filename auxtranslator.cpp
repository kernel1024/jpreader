#include <QThread>
#include <QScopedPointer>
#include "auxtranslator.h"
#include "specwidgets.h"
#include "globalcontrol.h"

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

void CAuxTranslator::translatePriv()
{
    if (!m_text.isEmpty()) {
        QScopedPointer<CAbstractTranslator> tran(CAbstractTranslator::translatorFactory(this, m_lang));
        if (tran.isNull() || !tran->initTran()) {
            qCritical() << tr("Unable to initialize translation engine.");
            m_text = QSL("ERROR");
        } else {
            QString ssrc = m_text;
            QString res;
            ssrc = ssrc.replace(QSL("\r\n"),QSL("\n"));
            ssrc = ssrc.replace('\r','\n');
            const QStringList sl = ssrc.split('\n',QString::KeepEmptyParts);
            for (const QString &s : qAsConst(sl)) {
                if (s.trimmed().isEmpty()) {
                    res.append('\n');
                } else {
                    QString r = tran->tranString(s);
                    if (r.trimmed().isEmpty()) {
                        res.append(s);
                    } else {
                        res.append(r);
                    }
                    res.append('\n');
                }
            }
            tran->doneTran();
            m_text = res;
        }
    }
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

