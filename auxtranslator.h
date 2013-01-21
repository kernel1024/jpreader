#ifndef AUXTRANSLATOR_H
#define AUXTRANSLATOR_H

#include <QtCore>
#include "atlastranslator.h"

class CAuxTranslator : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.jpreader.auxtranslator")
private:
    QString text;
    CAtlasTranslator::ATTranslateMode tranMode;
public:
    explicit CAuxTranslator(QObject *parent = 0);
    void setParams(const QString& Text, const CAtlasTranslator::ATTranslateMode TranMode =
            CAtlasTranslator::AutoTran);

signals:
    Q_SCRIPTABLE void gotTranslation(const QString& text);
    
public slots:
    void startTranslation();
    void setText(const QString& Text);

};

#endif // AUXTRANSLATOR_H
