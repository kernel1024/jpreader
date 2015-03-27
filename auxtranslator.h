#ifndef AUXTRANSLATOR_H
#define AUXTRANSLATOR_H

#include <QObject>
#include <QString>
#include "abstracttranslator.h"

class CAuxTranslator : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.jpreader.auxtranslator")
private:
    QString text;
public:
    explicit CAuxTranslator(QObject *parent = 0);
    void setParams(const QString& Text);

signals:
    Q_SCRIPTABLE void gotTranslation(const QString& text);
    
protected slots:
    void startTranslation(bool deleteAfter = true);

public slots:
    void startAuxTranslation(const QString& text);

};

#endif // AUXTRANSLATOR_H
