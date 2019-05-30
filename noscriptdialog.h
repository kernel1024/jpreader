#ifndef NOSCRIPTDIALOG_H
#define NOSCRIPTDIALOG_H

#include <QDialog>

namespace Ui {
class CNoScriptDialog;
}

class CNoScriptDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CNoScriptDialog(QWidget *parent, const QString& origin);
    ~CNoScriptDialog() override;

private:
    Ui::CNoScriptDialog *ui;
    QString m_origin;

    Q_DISABLE_COPY(CNoScriptDialog)

    void updateHostsList();

protected:
    void showEvent(QShowEvent* event) override;

private Q_SLOTS:
    void acceptScripts();

};

#endif // NOSCRIPTDIALOG_H
