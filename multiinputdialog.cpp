#include "multiinputdialog.h"
#include "ui_multiinputdialog.h"

CMultiInputDialog::CMultiInputDialog(QWidget *parent, const QString& title,
                                     const QStrHash& data) :
    QDialog(parent),
    ui(new Ui::CMultiInputDialog)
{
    ui->setupUi(this);

    setWindowTitle(title);

    formLayout = new QFormLayout();
    formLayout->setObjectName(QStringLiteral("formLayout"));

    int i = 0;
    foreach (const QString &key, data.keys()) {
        QLabel *label = new QLabel(this);
        label->setObjectName(QString("label_%1").arg(i));
        label->setText(key);
        labels << label;

        formLayout->setWidget(i, QFormLayout::LabelRole, label);

        QLineEdit *lineEdit = new QLineEdit(this);
        lineEdit->setObjectName(QString("lineEdit_%1").arg(i));
        lineEdit->setText(data.value(key));
        edits << lineEdit;

        formLayout->setWidget(i, QFormLayout::FieldRole, lineEdit);

        i++;
    }

    ui->verticalLayout->insertLayout(0,formLayout);
}

CMultiInputDialog::~CMultiInputDialog()
{
    delete ui;
}

QStrHash CMultiInputDialog::getInputData()
{
    QStrHash res;
    for (int i=0;i<edits.count();i++)
        res[labels.at(i)->text()] = edits.at(i)->text();
    return res;
}
