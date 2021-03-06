#include "multiinputdialog.h"
#include "ui_multiinputdialog.h"

CMultiInputDialog::CMultiInputDialog(QWidget *parent, const QString& title,
                                     const CStringHash& data, const QString& helperText) :
    QDialog(parent),
    ui(new Ui::CMultiInputDialog)
{
    ui->setupUi(this);

    setWindowTitle(title);

    formLayout = new QFormLayout(parent);
    formLayout->setObjectName(QSL("formLayout"));

    int i = 0;
    for (auto it = data.constBegin(), end = data.constEnd(); it != end; ++it) {
        auto *label = new QLabel(this);
        label->setObjectName(QSL("label_%1").arg(i));
        label->setText(it.key());
        labels << label;

        formLayout->setWidget(i, QFormLayout::LabelRole, label);

        auto *lineEdit = new QLineEdit(this);
        lineEdit->setObjectName(QSL("lineEdit_%1").arg(i));
        lineEdit->setText(it.value());
        edits << lineEdit;

        formLayout->setWidget(i, QFormLayout::FieldRole, lineEdit);

        i++;
    }

    ui->verticalLayout->insertLayout(0,formLayout);

    if (!helperText.isEmpty()) {
        auto *hlp = new QLabel(this);
        hlp->setObjectName(QSL("label_helper"));
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
