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
    ~CLightTranslator() override;
    void appendSourceText(const QString& text);

private:
    Ui::CLigthTranslator *ui;
    bool isTranslating { false };

    Q_DISABLE_COPY(CLightTranslator)

    void reloadLanguageList();

protected:
    void closeEvent(QCloseEvent * event) override;

public Q_SLOTS:
    void restoreWindow();
    void translate();
    void gotTranslation(const QString& text);

};

#endif // LIGHTTRANSLATOR_H
