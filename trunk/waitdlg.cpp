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

void CWaitDlg::setText(QString aMsg)
{
    ui->lblMessage->setText(aMsg);
}

void CWaitDlg::setProgressEnabled(bool show)
{
    ui->atlasFrame->setVisible(show);
}

void CWaitDlg::setProgressValue(int val)
{
    ui->barProgress->setValue(val);
}
