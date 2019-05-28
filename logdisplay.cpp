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
    delete ui;
}

void CLogDisplay::updateMessages()
{
    if (qApp->closingDown()) return;
    if (!isVisible()) return;
    if (debugMessages.isEmpty()) return;

    QString log = debugMessages.join('\n');
    debugMessages.clear();
    log.append('\n');

    ui->logView->moveCursor(QTextCursor::End);
    ui->logView->insertPlainText(log);
    ui->logView->moveCursor(QTextCursor::End);

    ui->linesCount->setText(QStringLiteral("%1 messages").arg(ui->logView->document()->lineCount()));
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

    u = QInputDialog::getText(this,tr("Add AdBlock rule"),
                              tr("Filter template"),QLineEdit::Normal,u,&ok);
    if (ok)
        gSet->adblockAppend(u);
}

void CLogDisplay::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

    updateMessages();
    if (firstShow && gSet && !gSet->mainWindows.isEmpty()) {
        QPoint p = gSet->mainWindows.constFirst()->pos();
        p.rx()+=200;
        p.ry()+=100;
        move(p);
        firstShow=false;
    }
}
