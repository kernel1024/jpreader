#include "multiinputdialog.h"
#include "ui_multiinputdialog.h"

CMultiInputDialog::CMultiInputDialog(QWidget *parent, const QString& title,
                                     const CStringHash& data, const QString& helperText) :
    QDialog(parent),
    m_ui(new Ui::CMultiInputDialog)
{
    m_ui->setupUi(this);

    setWindowTitle(title);

    m_formLayout = new QFormLayout(parent);
    m_formLayout->setObjectName(QSL("formLayout"));

    int i = 0;
    m_labels.reserve(data.size());
    m_edits.reserve(data.size());
    for (auto it = data.constBegin(), end = data.constEnd(); it != end; ++it) {
        auto *label = new QLabel(this);
        label->setObjectName(QSL("label_%1").arg(i));
        label->setText(it.key());
        m_labels.append(label);

        m_formLayout->setWidget(i, QFormLayout::LabelRole, label);

        auto *lineEdit = new QLineEdit(this);
        lineEdit->setObjectName(QSL("lineEdit_%1").arg(i));
        lineEdit->setText(it.value());
        m_edits.append(lineEdit);

        m_formLayout->setWidget(i, QFormLayout::FieldRole, lineEdit);

        i++;
    }

    m_ui->verticalLayout->insertLayout(0,m_formLayout);

    if (!helperText.isEmpty()) {
        auto *hlp = new QLabel(this);
        hlp->setObjectName(QSL("label_helper"));
        hlp->setText(helperText);
        m_ui->verticalLayout->insertWidget(0,hlp);
    }
}

CMultiInputDialog::~CMultiInputDialog()
{
    delete m_ui;
}

CStringHash CMultiInputDialog::getInputData()
{
    CStringHash res;
    for (int i=0;i<m_edits.count();i++)
        res[m_labels.at(i)->text()] = m_edits.at(i)->text();
    return res;
}
