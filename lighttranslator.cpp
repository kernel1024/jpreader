#include <QMessageBox>
#include "lighttranslator.h"
#include "ui_lighttranslator.h"
#include "globalcontrol.h"
#include "auxtranslator.h"
#include "structures.h"

CLightTranslator::CLightTranslator(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CLigthTranslator)
{
    isTranslating = false;

    ui->setupUi(this);

    ui->barTranslating->hide();
    ui->radioJap->setChecked(true);

    connect(ui->btnTranslate,&QPushButton::clicked,this,&CLightTranslator::translate);
}

void CLightTranslator::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void CLightTranslator::restoreWindow()
{
    show();
    activateWindow();
}

void CLightTranslator::translate()
{
    if (isTranslating) {
        QMessageBox::warning(this,tr("JPReader"),
                             tr("ATLAS engine is translating text now. Please try later."));
        return;
    }
    QString s = ui->textSource->toPlainText();
    if (s.isEmpty()) return;

    isTranslating = true;

    QThread *th = new QThread();
    CAuxTranslator *at = new CAuxTranslator();
    at->setParams(s);
    if (ui->radioJap->isChecked())
        at->setSrcLang(LS_JAPANESE);
    if (ui->radioEng->isChecked())
        at->setSrcLang(LS_ENGLISH);
    connect(this,&CLightTranslator::startTranslation,at,&CAuxTranslator::startTranslation,Qt::QueuedConnection);
    connect(at,&CAuxTranslator::gotTranslation,this,&CLightTranslator::gotTranslation,Qt::QueuedConnection);
    at->moveToThread(th);
    th->start();

    ui->barTranslating->show();

    emit startTranslation(true);
}

void CLightTranslator::gotTranslation(const QString &text)
{
    isTranslating = false;
    ui->barTranslating->hide();
    activateWindow();
    if (text.startsWith("ERROR")) {
        QMessageBox::warning(this,tr("JPReader"),
                             tr("Error occured during translation. Try again."));
        ui->textResult->clear();
        return;
    }
    ui->textResult->setPlainText(text);
}

CLightTranslator::~CLightTranslator()
{
    delete ui;
}

void CLightTranslator::appendSourceText(const QString &text)
{
    ui->textSource->appendPlainText(text);
}
