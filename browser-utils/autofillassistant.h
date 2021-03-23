#ifndef AUTOFILLASSISTANT_H
#define AUTOFILLASSISTANT_H

#include <QWidget>

namespace Ui {
class CAutofillAssistant;
}

class CAutofillAssistant : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(CAutofillAssistant)

public:
    explicit CAutofillAssistant(QWidget *parent = nullptr);
    ~CAutofillAssistant() override;

public Q_SLOTS:
    void pasteAndForward();
    void loadFromFile();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::CAutofillAssistant *ui;
    bool m_firstResize { true };
};

#endif // AUTOFILLASSISTANT_H
