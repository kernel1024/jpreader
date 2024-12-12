#ifndef MULTIINPUTDIALOG_H
#define MULTIINPUTDIALOG_H

#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include "global/structures.h"

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
    Ui::CMultiInputDialog *m_ui;

    QFormLayout *m_formLayout;
    QList<QLabel *> m_labels;
    QList<QLineEdit *> m_edits;

    Q_DISABLE_COPY(CMultiInputDialog)

};

#endif // MULTIINPUTDIALOG_H
