#include <QScrollBar>
#include <QDebug>
#include <QInputDialog>
#include "logdisplay.h"
#include "genericfuncs.h"
#include "globalcontrol.h"
#include "mainwindow.h"
#include "specwidgets.h"
#include "ui_logdisplay.h"

CLogDisplay::CLogDisplay() :
    QDialog(NULL, Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint),
    ui(new Ui::CLogDisplay)
{
    ui->setupUi(this);
    firstShow = true;
    syntax = new CSpecLogHighlighter(ui->logView->document());

    ui->logView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->logView,SIGNAL(customContextMenuRequested(QPoint)),
            this,SLOT(logCtxMenu(QPoint)));

    updateMessages();
}

CLogDisplay::~CLogDisplay()
{
    savedMessages.clear();
    delete ui;
}

void CLogDisplay::updateMessages()
{
    if (!isVisible()) return;
    int fr = -1;
    int sv = -1;
    if (ui->logView->verticalScrollBar()!=NULL)
        sv = ui->logView->verticalScrollBar()->value();

    if (!savedMessages.isEmpty())
        fr = debugMessages.lastIndexOf(savedMessages.last());
    if (fr>=0 && fr<debugMessages.count()) {
        for (int i=(fr+1);i<debugMessages.count();i++)
            savedMessages << debugMessages.at(i);
    } else
        savedMessages = debugMessages;

    updateText(savedMessages.join('\n'));
    if (ui->logView->verticalScrollBar()!=NULL) {
        if (!ui->checkScrollLock->isChecked())
            ui->logView->verticalScrollBar()->setValue(ui->logView->verticalScrollBar()->maximum());
        else if (sv!=-1)
            ui->logView->verticalScrollBar()->setValue(sv);
    }
}

void CLogDisplay::logCtxMenu(const QPoint &pos)
{
    QMenu *cm = ui->logView->createStandardContextMenu();

    QUrl su(ui->logView->textCursor().selectedText());
    if (su.isValid()) {
        cm->addSeparator();

        QAction* ac = cm->addAction(QIcon::fromTheme("emblem-important"),
                                    tr("Adblock this URL"),this,SLOT(addToAdblock()));
        ac->setData(su);
    }

    cm->setAttribute(Qt::WA_DeleteOnClose,true);
    cm->popup(ui->logView->mapToGlobal(pos));
}

void CLogDisplay::addToAdblock()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QUrl url = nt->data().toUrl();
    if (url.isEmpty() || !url.isValid()) return;
    QString u = url.toString();
    bool ok;

    u = QInputDialog::getText(this,tr("Add adblock filter"),tr("Filter template"),QLineEdit::Normal,u,&ok);
    if (ok)
        gSet->adblockAppend(u);
}

void CLogDisplay::updateText(const QString &text)
{
    ui->logView->setPlainText(text);
}

void CLogDisplay::showEvent(QShowEvent *)
{
    updateMessages();
    if (firstShow && gSet!=NULL && !gSet->mainWindows.isEmpty()) {
        QPoint p = gSet->mainWindows.first()->pos();
        p.rx()+=200;
        p.ry()+=100;
        move(p);
        firstShow=false;
    }
}
