#include "multiinputdialog.h"
#include "ui_multiinputdialog.h"

CMultiInputDialog::CMultiInputDialog(QWidget *parent, const QString& title,
                                     const CStringHash& data, const QString& helperText) :
    QDialog(parent),
    ui(new Ui::CMultiInputDialog)
{
    ui->setupUi(this);

    setWindowTitle(title);

    formLayout = new QFormLayout();
    formLayout->setObjectName(QStringLiteral("formLayout"));

    int i = 0;
    for (auto it = data.constBegin(), end = data.constEnd(); it != end; ++it) {
        QLabel *label = new QLabel(this);
        label->setObjectName(QString(QStringLiteral("label_%1")).arg(i));
        label->setText(it.key());
        labels << label;

        formLayout->setWidget(i, QFormLayout::LabelRole, label);

        auto lineEdit = new QLineEdit(this);
        lineEdit->setObjectName(QString(QStringLiteral("lineEdit_%1")).arg(i));
        lineEdit->setText(it.value());
        edits << lineEdit;

        formLayout->setWidget(i, QFormLayout::FieldRole, lineEdit);

        i++;
    }

    ui->verticalLayout->insertLayout(0,formLayout);

    if (!helperText.isEmpty()) {
        QLabel* hlp = new QLabel(this);
        hlp->setObjectName(QStringLiteral("label_helper"));
        hlp->setText(helperText);
        ui->verticalLayout->insertWidget(0,hlp);
    }
}

CMultiInputDialog::~CMultiInputDialog()
{
    delete ui;
}

CStringHash CMultiInputDialog::getInputData()
{
    CStringHash res;
    for (int i=0;i<edits.count();i++)
        res[labels.at(i)->text()] = edits.at(i)->text();
    return res;
}
