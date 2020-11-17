#include <QMessageBox>
#include <QThread>
#include "lighttranslator.h"
#include "ui_lighttranslator.h"
#include "global/globalcontrol.h"
#include "global/structures.h"
#include "auxtranslator.h"

CLightTranslator::CLightTranslator(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CLigthTranslator)
{
    ui->setupUi(this);

    ui->barTranslating->hide();

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
    reloadLanguageList();
}

void CLightTranslator::translate()
{
    if (isTranslating) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Translation engine is busy now. Please try later."));
        return;
    }
    QString s = ui->textSource->toPlainText();
    if (s.isEmpty()) return;

    CLangPair lp = CLangPair(ui->comboLanguage->currentData().toString());
    if (!lp.isValid()) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Unable to initialize translation engine. Unacceptable language pair."));
        return;
    }
    isTranslating = true;

    auto *th = new QThread();
    auto *at = new CAuxTranslator();
    at->setText(s);
    at->setSrcLang(lp.langFrom.bcp47Name());
    at->setDestLang(lp.langTo.bcp47Name());
    at->moveToThread(th);
    connect(at,&CAuxTranslator::gotTranslation,this,&CLightTranslator::gotTranslation,Qt::QueuedConnection);
    connect(th,&QThread::finished,at,&CAuxTranslator::deleteLater);
    connect(th,&QThread::finished,th,&QThread::deleteLater);
    th->setObjectName(QSL("LightTran"));
    th->start();

    ui->barTranslating->show();

    QMetaObject::invokeMethod(at,&CAuxTranslator::translateAndQuit,Qt::QueuedConnection);
}

void CLightTranslator::gotTranslation(const QString &text)
{
    isTranslating = false;
    ui->barTranslating->hide();
    activateWindow();
    if (text.startsWith(QSL("ERROR"))) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
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

void CLightTranslator::reloadLanguageList()
{
    for (const CLangPair& pair : qAsConst(gSet->settings()->translatorPairs)) {
        ui->comboLanguage->addItem(QSL("%1 - %2").arg(
                                      gSet->getLanguageName(pair.langFrom.bcp47Name()),
                                      gSet->getLanguageName(pair.langTo.bcp47Name())),
                                   pair.getHash());
    }
    if (gSet->ui()->languageSelector->checkedAction()) {
        QString selectedHash = gSet->ui()->languageSelector->checkedAction()->data().toString();
        int idx = ui->comboLanguage->findData(selectedHash);
        if (idx>=0)
            ui->comboLanguage->setCurrentIndex(idx);
    }
}
