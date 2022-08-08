#include "authdlg.h"
#include "global/control.h"
#include "global/browserfuncs.h"
#include "ui_authdlg.h"

CAuthDlg::CAuthDlg(QWidget *parent, const QUrl &origin, const QString &realm,
                   bool autofillLogin) :
    QDialog(parent),
    ui(new Ui::AuthDlg),
    m_origin(origin),
    m_realm(realm),
    m_autofillLogin(autofillLogin)
{
    ui->setupUi(this);

    if (!realm.isEmpty()) {
        ui->labelRealm->setText(realm);
    } else if (origin.isValid()) {
        ui->labelRealm->setText(origin.toString());
    } else {
        ui->label->clear();
    }

    if (origin.isValid()) {
        QString user;
        QString pass;
        gSet->browser()->readPassword(origin,realm,user,pass);
        ui->lineUser->setText(user);
        ui->linePassword->setText(pass);
    }

    if (m_autofillLogin) {
        ui->checkSavePassword->setEnabled(false);
        ui->checkSavePassword->setChecked(true);
    }

    connect(ui->OKBtn,&QPushButton::clicked,this,&CAuthDlg::acceptPass);
    connect(ui->CancelBtn,&QPushButton::clicked,this,&CAuthDlg::reject);
}

CAuthDlg::~CAuthDlg()
{
    delete ui;
}

QString CAuthDlg::getUser()
{
    return ui->lineUser->text();
}

QString CAuthDlg::getPassword()
{
    return ui->linePassword->text();
}

void CAuthDlg::acceptPass()
{
    if (ui->checkSavePassword->isChecked() && m_origin.isValid())
        gSet->browser()->savePassword(m_origin,m_realm,getUser(),getPassword());
    accept();
}

