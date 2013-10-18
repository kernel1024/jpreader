#include "waitdlg.h"
#include "ui_waitdlg.h"

CWaitDlg::CWaitDlg(QWidget *parent, QString aMsg) :
    QWidget(parent),
    ui(new Ui::WaitDlg)
{
    ui->setupUi(this);
    ui->lblMessage->setText(aMsg);
    setProgressEnabled(false);
    abortBtn=ui->abortBtn;
}

CWaitDlg::~CWaitDlg()
{
    delete ui;
}
