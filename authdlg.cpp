#include "authdlg.h"
#include "globalcontrol.h"
#include "ui_authdlg.h"

CAuthDlg::CAuthDlg(QWidget *parent, const QUrl &origin, const QString &realm) :
    QDialog(parent),
    ui(new Ui::AuthDlg)
{
    ui->setupUi(this);

    u_origin = origin;

    if (!realm.isEmpty())
        ui->labelRealm->setText(realm);
    else if (origin.isValid())
        ui->labelRealm->setText(origin.toString());
    else
        ui->label->clear();

    if (origin.isValid()) {
        QString user = "";
        QString pass = "";
        gSet->readPassword(origin,user,pass);
        ui->lineUser->setText(user);
        ui->linePassword->setText(pass);
    }

    connect(ui->OKBtn,SIGNAL(clicked()),this,SLOT(acceptPass()));
    connect(ui->CancelBtn,SIGNAL(clicked()),this,SLOT(reject()));
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
    if (ui->checkSavePassword->isChecked() && u_origin.isValid())
        gSet->savePassword(u_origin,getUser(),getPassword());
    accept();
}

