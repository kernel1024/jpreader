#ifndef LIGHTTRANSLATOR_H
#define LIGHTTRANSLATOR_H

#include <QDialog>
#include <QString>
#include "abstracttranslator.h"

namespace Ui {
class CLigthTranslator;
}

class CLightTranslator : public QDialog
{
    Q_OBJECT
    
public:
    explicit CLightTranslator(QWidget *parent = nullptr);
    ~CLightTranslator();
    void appendSourceText(const QString& text);

private:
    Ui::CLigthTranslator *ui;
    bool isTranslating;
    void reloadLanguageList();

protected:
    void closeEvent(QCloseEvent * event);

signals:
    void startTranslation(bool deleteAfter);

public slots:
    void restoreWindow();
    void translate();
    void gotTranslation(const QString& text);

};

#endif // LIGHTTRANSLATOR_H
