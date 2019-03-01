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
    QDialog(nullptr, Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint),
    ui(new Ui::CLogDisplay)
{
    ui->setupUi(this);
    firstShow = true;
    syntax = new CSpecLogHighlighter(ui->logView->document());

    ui->logView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->logView, &QTextEdit::customContextMenuRequested,
            this, &CLogDisplay::logCtxMenu);

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
    if (ui->logView->verticalScrollBar())
        sv = ui->logView->verticalScrollBar()->value();

    if (!savedMessages.isEmpty())
        fr = debugMessages.lastIndexOf(savedMessages.constLast());
    if (fr>=0 && fr<debugMessages.count()) {
        for (int i=(fr+1);i<debugMessages.count();i++)
            savedMessages << debugMessages.at(i);
    } else
        savedMessages = debugMessages;

    updateText(savedMessages.join('\n'));
    if (ui->logView->verticalScrollBar()) {
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

        QAction* ac = cm->addAction(QIcon::fromTheme(QStringLiteral("preferences-web-browser-adblock")),
                                    tr("Add AdBlock rule for url..."),this,
                                    &CLogDisplay::addToAdblock);
        ac->setData(su);
    }

    cm->setAttribute(Qt::WA_DeleteOnClose,true);
    cm->popup(ui->logView->mapToGlobal(pos));
}

void CLogDisplay::addToAdblock()
{
    auto nt = qobject_cast<QAction *>(sender());
    if (nt==nullptr) return;
    QUrl url = nt->data().toUrl();
    if (url.isEmpty() || !url.isValid()) return;
    QString u = url.toString();
    bool ok;

    u = QInputDialog::getText(this,tr("Add AdBlock rule"),tr("Filter template"),QLineEdit::Normal,u,&ok);
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
    if (firstShow && gSet && !gSet->mainWindows.isEmpty()) {
        QPoint p = gSet->mainWindows.constFirst()->pos();
        p.rx()+=200;
        p.ry()+=100;
        move(p);
        firstShow=false;
    }
}
