#include "lighttranslator.h"
#include "ui_lighttranslator.h"
#include "globalcontrol.h"
#include "auxtranslator.h"

CLightTranslator::CLightTranslator(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CLigthTranslator)
{
    tranMode = CAtlasTranslator::AutoTran;
    isTranslating = false;

    ui->setupUi(this);

    ui->barTranslating->hide();

    connect(ui->btnTranslate,SIGNAL(clicked()),this,SLOT(translate()));
    connect(ui->rbDirAuto,SIGNAL(toggled(bool)),this,SLOT(tranModeChanged(bool)));

    ui->rbDirAuto->setChecked(true);
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
        QMessageBox::warning(this,tr("JPReader error"),tr("ATLAS engine is translating text now. Please try later."));
        return;
    }
    QString s = ui->textSource->toPlainText();
    if (s.isEmpty()) return;

    isTranslating = true;

    QThread *th = new QThread();
    CAuxTranslator *at = new CAuxTranslator();
    at->setParams(s,tranMode);
    connect(this,SIGNAL(startTranslation()),at,SLOT(startTranslationOnce()),Qt::QueuedConnection);
    connect(at,SIGNAL(gotTranslation(QString)),this,SLOT(gotTranslation(QString)),Qt::QueuedConnection);
    at->moveToThread(th);
    th->start();

    ui->barTranslating->show();

    emit startTranslation();
}

void CLightTranslator::tranModeChanged(bool)
{
    CAtlasTranslator::ATTranslateMode m = CAtlasTranslator::AutoTran;
    if (ui->rbDirEJ->isChecked())
        m = CAtlasTranslator::EngToJpnTran;
    else if (ui->rbDirJE->isChecked())
        m = CAtlasTranslator::JpnToEngTran;
    tranMode = m;
}

void CLightTranslator::gotTranslation(const QString &text)
{
    isTranslating = false;
    ui->barTranslating->hide();
    activateWindow();
    if (text.startsWith("ERROR")) {
        QMessageBox::warning(this,tr("JPReader error"),tr("Error occured during translation. Try again."));
        ui->textResult->clear();
        return;
    }
    ui->textResult->setPlainText(text);
}

CLightTranslator::~CLightTranslator()
{
    delete ui;
}
