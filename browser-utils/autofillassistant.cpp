#include <QTextCursor>
#include <QTextBlock>
#include <QMessageBox>
#include <QSettings>

#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/ui.h"
#include "mainwindow.h"

#include "autofillassistant.h"
#include "ui_autofillassistant.h"

CAutofillAssistant::CAutofillAssistant(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CAutofillAssistant)
{
    ui->setupUi(this);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);

    ui->editor->setBracketsMatchingEnabled(false);
    ui->editor->setCodeFoldingEnabled(false);
    ui->editor->setLineNumbersVisible(true);
    ui->editor->setTextWrapEnabled(false);
    ui->editor->setKeywords(QStringList());

    connect(ui->buttonPaste,&QPushButton::clicked,this,&CAutofillAssistant::pasteAndForward);
    connect(ui->buttonLoad,&QPushButton::clicked,this,&CAutofillAssistant::loadFromFile);

    QSettings stg;
    stg.beginGroup(QSL("AutofillAssistant"));
    ui->checkSendTab->setChecked(stg.value(QSL("sendTab"),true).toBool());
    stg.endGroup();
}

CAutofillAssistant::~CAutofillAssistant()
{
    QSettings stg;
    stg.beginGroup(QSL("AutofillAssistant"));
    stg.setValue(QSL("sendTab"), ui->checkSendTab->isChecked());
    stg.endGroup();

    delete ui;
}

void CAutofillAssistant::pasteAndForward()
{
    const QChar tabKey(0x9);

    if (ui->editor->textCursor().atEnd()) return;

    QTextCursor cursor = ui->editor->textCursor();
    cursor.clearSelection();
    QString data = cursor.block().text();
    cursor.movePosition(QTextCursor::MoveOperation::NextBlock);
    cursor.select(QTextCursor::SelectionType::BlockUnderCursor);
    ui->editor->setTextCursor(cursor);

    if (ui->checkSendTab->isChecked())
        data += tabKey;

    CMainWindow* mw = gSet->activeWindow();
    if (mw && mw->sendInputToActiveBrowser(data))
        return;

    QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                         tr("Unable to find active main window with active browser tab."));
}

void CAutofillAssistant::loadFromFile()
{
    const QString fname = CGenericFuncs::getOpenFileNameD(this,tr("Open text file"),gSet->settings()->savedAuxDir);
    if (fname.isEmpty()) return;

    gSet->ui()->setSavedAuxDir(QFileInfo(fname).absolutePath());

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Unable to open file %1.").arg(fname));
        return;
    }
    ui->editor->setPlainText(CGenericFuncs::detectDecodeToUnicode(f.readAll()));
    f.close();
}

void CAutofillAssistant::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void CAutofillAssistant::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

    const int horizontalWindowMargin = 20;
    const int verticalWindowMargin = 120;

    if (!m_firstResize) return;
    m_firstResize = false;

    QRect geom = gSet->ui()->getLastMainWindowGeometry();
    if (!geom.isNull()) {
        QPoint p = geom.topRight() + QPoint(-1 * (horizontalWindowMargin + width()),
                                            verticalWindowMargin);
        move(p);
    }
}
