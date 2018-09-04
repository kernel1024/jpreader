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
public:
    explicit CAuxTranslator(QObject *parent = nullptr);

signals:
    Q_SCRIPTABLE void gotTranslation(const QString& text);
    
public slots:
    void setText(const QString& text);
    void setSrcLang(const QString& lang);
    void setDestLang(const QString& lang);
    void startTranslation(bool deleteAfter);
    void startAuxTranslation(const QString& text);

};

#endif // AUXTRANSLATOR_H
