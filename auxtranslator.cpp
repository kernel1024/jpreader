#include "auxtranslator.h"
#include "specwidgets.h"
#include "globalcontrol.h"

CAuxTranslator::CAuxTranslator(QObject *parent) :
    QObject(parent)
{
    m_text = QString();
    m_lang = CLangPair("ja","en");
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

void CAuxTranslator::startTranslation(bool deleteAfter)
{
    if (!m_text.isEmpty()) {
        CAbstractTranslator* tran=translatorFactory(this, m_lang);
        if (tran==nullptr || !tran->initTran()) {
            qCritical() << tr("Unable to initialize translation engine.");
            m_text = QLatin1String("ERROR");
        } else {
            QString ssrc = m_text;
            QString res;
            ssrc = ssrc.replace("\r\n","\n");
            ssrc = ssrc.replace('\r','\n');
            const QStringList sl = ssrc.split('\n',QString::KeepEmptyParts);
            for (const QString &s : qAsConst(sl)) {
                if (s.trimmed().isEmpty())
                    res.append('\n');
                else {
                    QString r = tran->tranString(s);
                    if (r.trimmed().isEmpty())
                        res.append(s);
                    else
                        res.append(r);
                    res.append('\n');
                }
            }
            tran->doneTran();
            m_text = res;
        }
        if (tran)
            tran->deleteLater();
    }
    emit gotTranslation(m_text);
    if (deleteAfter)
        deleteLater();
}

void CAuxTranslator::startAuxTranslation(const QString &text)
{
    setText(text);
    startTranslation(false);
}

