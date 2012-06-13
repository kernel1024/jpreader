#ifndef LIGHTTRANSLATOR_H
#define LIGHTTRANSLATOR_H

#include <QtCore>
#include <QtGui>
#include "atlastranslator.h"

namespace Ui {
class CLigthTranslator;
}

class CLightTranslator : public QDialog
{
    Q_OBJECT
    
public:
    explicit CLightTranslator(QWidget *parent = 0);
    ~CLightTranslator();
    
private:
    Ui::CLigthTranslator *ui;
    CAtlasTranslator::ATTranslateMode tranMode;
    bool isTranslating;

protected:
    void closeEvent(QCloseEvent * event);

signals:
    void startTranslation();

public slots:
    void restoreWindow();
    void translate();
    void tranModeChanged(bool state);
    void gotTranslation(const QString& text);

};

#endif // LIGHTTRANSLATOR_H
