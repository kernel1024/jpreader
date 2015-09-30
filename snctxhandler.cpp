#include <QImageWriter>
#include <QMessageBox>
#include <QInputDialog>
#include <QProcess>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QClipboard>
#include <QMimeData>

#include "qxttooltip.h"
#include "snctxhandler.h"
#include "genericfuncs.h"
#include "bookmarkdlg.h"
#include "searchtab.h"
#include "specwidgets.h"
#include "auxtranslator.h"

CSnCtxHandler::CSnCtxHandler(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    menuActive.setSingleShot(false);
    connect(&menuActive, &QTimer::timeout,[this](){
        emit hideTooltips();
    });
}

void CSnCtxHandler::contextMenu(const QPoint &pos)
{
    QString sText = snv->txtBrowser->selectedText();

    QAction *ac;

    QMenu *cm = new QMenu(snv);

    if (!sText.isEmpty()) {
        cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::Copy));

        ac = new QAction(QIcon::fromTheme("document-edit-verify"),tr("Translate"),NULL);
        ac->setData(sText);
        connect(ac, &QAction::triggered,this, &CSnCtxHandler::translateFragment);
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("tab-new-background"),tr("Create plain text in separate tab"),NULL);
        connect(ac, &QAction::triggered, [sText,this](){
            QString s = sText;
            s = s.replace('\n',"<br/>");
            new CSnippetViewer(snv->parentWnd,QUrl(),QStringList(),true,s);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("tab-new-background"),tr("Translate plain text in separate tab"),NULL);
        connect(ac, &QAction::triggered, [sText,this](){
            QString s = sText;
            s = s.replace('\n',"<br/>");
            CSnippetViewer* sn = new CSnippetViewer(snv->parentWnd,QUrl(),QStringList(),true,s);
            sn->requestAutotranslate = true;
        });
        cm->addAction(ac);
    }

    if (!sText.isEmpty()) {
        cm->addSeparator();
        ac = new QAction(QIcon(":/img/google"),tr("Search in Google"),NULL);
        connect(ac, &QAction::triggered, [sText,this](){
            QUrl u("http://google.com/search");
            QUrlQuery qu;
            qu.addQueryItem("q", sText);
            u.setQuery(qu);
            new CSnippetViewer(snv->parentWnd, u);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon(":/img/nepomuk"),tr("Local search"),NULL);
        connect(ac, &QAction::triggered, [sText,this](){
            CSearchTab *bt = new CSearchTab(snv->parentWnd);
            bt->searchTerm(sText);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon(":/img/jisho"),tr("Jisho word translation"),NULL);
        connect(ac, &QAction::triggered, [sText,this](){
            QUrl u("http://jisho.org/words");
            QUrlQuery qu;
            qu.addQueryItem("jap",sText);
            qu.addQueryItem("eng=","");
            qu.addQueryItem("dict","edict");
            u.setQuery(qu);
            new CSnippetViewer(snv->parentWnd, u);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("accessories-dictionary"),tr("Local dictionary"),NULL);
        connect(ac, &QAction::triggered, [sText](){
            gSet->showDictionaryWindow(sText);
        });
        cm->addAction(ac);
    }

    cm->addSeparator();
    ac = new QAction(QIcon::fromTheme("tab-duplicate"),tr("Duplicate tab"),NULL);
    connect(ac, &QAction::triggered, [this](){
        QString url = "about://blank";
        if (!snv->fileChanged) url=snv->urlEdit->text();

        CSnippetViewer* sv = new CSnippetViewer(snv->parentWnd, url);

        if (snv->fileChanged || url.isEmpty()) {
            snv->txtBrowser->page()->toHtml([sv,this](const QString& html) {
                if (!html.isEmpty())
                    sv->txtBrowser->setHtml(html,snv->txtBrowser->page()->url());
            });
        }
    });
    cm->addAction(ac);

    cm->addAction(QIcon::fromTheme("tab-detach"),tr("Detach tab"),
                 snv,SLOT(detachTab()));

    cm->addSeparator();
    cm->addAction(QIcon::fromTheme("bookmark-new"),tr("Add current page to bookmarks"),
                 this, SLOT(bookmarkPage()));

    cm->addSeparator();
    cm->addAction(QIcon::fromTheme("go-previous"),tr("Back"),
                 snv->txtBrowser,SLOT(back()),QKeySequence(Qt::CTRL + Qt::Key_Z));
    cm->addAction(QIcon::fromTheme("view-refresh"),tr("Reload"),
                 snv->txtBrowser,SLOT(reload()),QKeySequence(Qt::CTRL + Qt::Key_R));

    QClipboard *cb = QApplication::clipboard();
    if (cb->mimeData(QClipboard::Clipboard)->hasText())
        cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::Paste));
    cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::SelectAll));
    cm->addSeparator();

    QMenu *ccm = cm->addMenu(QIcon::fromTheme("system-run"),tr("Service"));
    ccm->addAction(QIcon::fromTheme("documentation"),tr("Show source"),
                 this,SLOT(loadKwrite()),QKeySequence(Qt::CTRL + Qt::Key_S));

    ac = new QAction(QIcon::fromTheme("download"),tr("Open in browser"),NULL);
    ac->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
    connect(ac, &QAction::triggered,[this](){
        if (!QProcess::startDetached(gSet->sysBrowser,
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start browser."));
    });
    ccm->addAction(ac);

    ccm->addAction(QIcon::fromTheme("document-edit-decrypt"),tr("Parse document"),
                   snv->transHandler,SLOT(reparseDocument()));
    ccm->addSeparator();
    ccm->addAction(snv->parentWnd->actionFullscreen);
    ccm->addAction(QIcon::fromTheme("dialog-close"),tr("Close tab"),
                 snv,SLOT(closeTab()));
    ccm->addSeparator();
    if (!sText.isEmpty()) {
        ac = ccm->addAction(QIcon::fromTheme("document-save-as"),tr("Save selected text as file..."),
                                      this,SLOT(saveToFile()));
        ac->setData(sText);
    }
    ccm->addAction(QIcon::fromTheme("document-save"),tr("Save to file..."),
                 this,SLOT(saveToFile()));

    menuActive.setInterval(1000);
    menuActive.start();

    connect(cm, &QMenu::aboutToHide, [this](){
        menuActive.stop();
    });

    cm->setAttribute(Qt::WA_DeleteOnClose,true);
    cm->popup(snv->txtBrowser->mapToGlobal(pos));
}

void CSnCtxHandler::translateFragment()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();
    if (s.isEmpty()) return;

    QThread *th = new QThread();
    CAuxTranslator *at = new CAuxTranslator();
    at->setParams(s);
    connect(this,SIGNAL(startTranslation()),at,SLOT(startTranslation()),Qt::QueuedConnection);

    connect(at,&CAuxTranslator::gotTranslation,this,[this](const QString& text){
        if (!text.isEmpty() && !menuActive.isActive()) {
            CSpecToolTipLabel* lbl = new CSpecToolTipLabel(wordWrap(text,80));
            lbl->setStyleSheet("QLabel { background: #fefdeb; color: black; }");
            connect(this, &CSnCtxHandler::hideTooltips,lbl, &CSpecToolTipLabel::close);
            QxtToolTip::show(QCursor::pos(),lbl,snv->parentWnd);
        }
    },Qt::QueuedConnection);

    at->moveToThread(th);
    th->start();

    emit startTranslation();
}

void CSnCtxHandler::saveToFile()
{
    QString selectedText;
    selectedText.clear();
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt!=NULL)
        selectedText = nt->data().toString();

    QString fname;
    if (selectedText.isEmpty())
        fname = getSaveFileNameD(snv,tr("Save to HTML"),QDir::homePath(),
                                                 tr("HTML file (*.html);;Text file (*.txt)"));
    else
        fname = getSaveFileNameD(snv,tr("Save to file"),QDir::homePath(),
                                                 tr("Text file (*.txt)"));

    if (fname.isNull() || fname.isEmpty()) return;

    QFileInfo fi(fname);
    if (!selectedText.isEmpty()) {
        QFile f(fname);
        f.open(QIODevice::WriteOnly|QIODevice::Truncate);
        f.write(selectedText.toUtf8());
        f.close();
    } else if (fi.suffix().toLower()=="txt")
        snv->txtBrowser->page()->toPlainText([fname](const QString& result)
        {
            QFile f(fname);
            f.open(QIODevice::WriteOnly|QIODevice::Truncate);
            f.write(result.toUtf8());
            f.close();
        });
    else
        snv->txtBrowser->page()->toHtml([fname](const QString& result)
        {
            QFile f(fname);
            f.open(QIODevice::WriteOnly|QIODevice::Truncate);
            f.write(result.toUtf8());
            f.close();
        });
}

void CSnCtxHandler::bookmarkPage() {
    QWebEnginePage *lf = snv->txtBrowser->page();
    CBookmarkDlg *dlg = new CBookmarkDlg(snv,lf->title(),lf->requestedUrl().toString());
    if (dlg->exec()) {
        QString t = dlg->getBkTitle();
        if (!t.isEmpty() && !gSet->bookmarks.contains(t)) {
            gSet->bookmarks[t]=QUrl::fromUserInput(dlg->getBkUrl());
            gSet->updateAllBookmarks();
        } else
            QMessageBox::warning(snv,tr("JPReader"),
                                 tr("Unable to add bookmark (frame is empty or duplicate title). Try again."));
    }
    dlg->setParent(NULL);
    delete dlg;
}

void CSnCtxHandler::loadKwrite()
{
    QString fname = snv->getUrl().toLocalFile();
    if (fname.isEmpty() || snv->fileChanged)
    {
        QString uuid = QUuid::createUuid().toString().remove(QRegExp("[^a-z,A-Z,0,1-9,-]"))+".html";
        fname = QDir::temp().absoluteFilePath(uuid);
        snv->txtBrowser->page()->toHtml([fname,this](const QString& html) {
            QFile tfile(fname);
            tfile.open(QIODevice::WriteOnly);
            tfile.write(html.toUtf8());
            tfile.close();
            gSet->createdFiles.append(fname);

            if (!QProcess::startDetached(gSet->sysEditor, QStringList() << fname))
                QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start editor."));
        });
    }
}
