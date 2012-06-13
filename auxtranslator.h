#ifndef AUXTRANSLATOR_H
#define AUXTRANSLATOR_H

#include <QtCore>
#include "atlastranslator.h"

class CAuxTranslator : public QObject
{
    Q_OBJECT
private:
    QString text;
    CAtlasTranslator::ATTranslateMode tranMode;
public:
    explicit CAuxTranslator(QObject *parent = 0);
    void setParams(const QString& Text, const CAtlasTranslator::ATTranslateMode TranMode = CAtlasTranslator::AutoTran);
    
signals:
    void gotTranslation(const QString& text);
    
public slots:
    void startTranslation();
    
};

#endif // AUXTRANSLATOR_H
