#include "bookmarkdlg.h"
#include "ui_bookmarkdlg.h"

CBookmarkDlg::CBookmarkDlg(QWidget *parent, const QString &name, const QString &url) :
    QDialog(parent),
    ui(new Ui::BookmarkDlg)
{
    ui->setupUi(this);
    ui->lineTitle->setText(name);
    ui->lineUrl->setText(url);
}

CBookmarkDlg::~CBookmarkDlg()
{
    delete ui;
}

QString CBookmarkDlg::getBkTitle()
{
    return ui->lineTitle->text();
}

QString CBookmarkDlg::getBkUrl()
{
    return ui->lineUrl->text();
}
