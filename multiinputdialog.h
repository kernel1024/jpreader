#ifndef MULTIINPUTDIALOG_H
#define MULTIINPUTDIALOG_H

#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include "globalcontrol.h"

namespace Ui {
class CMultiInputDialog;
}

class CMultiInputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CMultiInputDialog(QWidget *parent, const QString &title,
                               const CStringHash& data, const QString &helperText = QString());
    ~CMultiInputDialog() override;

    CStringHash getInputData();

private:
    Ui::CMultiInputDialog *ui;

    QFormLayout *formLayout;
    QList<QLabel *> labels;
    QList<QLineEdit *> edits;

    Q_DISABLE_COPY(CMultiInputDialog)

};

#endif // MULTIINPUTDIALOG_H
