#ifndef AUXTRANSLATOR_H
#define AUXTRANSLATOR_H

#include <QObject>
#include <QString>
#include "abstracttranslator.h"

class CAuxTranslator : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kernel1024.jpreader.auxtranslator")
private:
    QString m_text;
    CLangPair m_lang;
    void translatePriv();
public:
    explicit CAuxTranslator(QObject *parent = nullptr);

Q_SIGNALS:
    Q_SCRIPTABLE void gotTranslation(const QString& text);
    
public Q_SLOTS:
    void setText(const QString& text);
    void setSrcLang(const QString& lang);
    void setDestLang(const QString& lang);
    void translateAndQuit();
    void startAuxTranslation(const QString& text);

};

#endif // AUXTRANSLATOR_H
