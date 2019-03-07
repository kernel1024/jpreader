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
    ~CNoScriptDialog();

private:
    Ui::CNoScriptDialog *ui;
    QString m_origin;

    void updateHostsList();

protected:
    void showEvent(QShowEvent* event);

private slots:
    void acceptScripts();
};

#endif // NOSCRIPTDIALOG_H
