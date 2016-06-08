#include <QImageWriter>
#include <QMessageBox>
#include <QInputDialog>
#include <QProcess>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QClipboard>
#include <QMimeData>
#include <QStringList>

#include "qxttooltip.h"
#include "snctxhandler.h"
#include "genericfuncs.h"
#include "bookmarkdlg.h"
#include "searchtab.h"
#include "specwidgets.h"
#include "auxtranslator.h"
#include "sourceviewer.h"

CSnCtxHandler::CSnCtxHandler(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    menuActive = new QTimer(this);
    menuActive->setSingleShot(false);
    connect(menuActive, &QTimer::timeout,[this](){
        emit hideTooltips();
    });
}

void CSnCtxHandler::contextMenu(const QPoint &pos)
{
    QString sText = snv->txtBrowser->selectedText();
    QUrl linkUrl = snv->msgHandler->hoveredLink;

    QClipboard *cb = QApplication::clipboard();
    QAction *ac;
    QMenu *cm = new QMenu(snv);

    if (linkUrl.isValid()) {
        ac = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new tab"),NULL);
        connect(ac, &QAction::triggered, [linkUrl,this]() {
            new CSnippetViewer(snv->parentWnd,linkUrl,QStringList(),false);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new tab and translate"),NULL);
        connect(ac, &QAction::triggered, [linkUrl,this]() {
            CSnippetViewer* sn = new CSnippetViewer(snv->parentWnd,linkUrl,QStringList(),false);
            sn->requestAutotranslate = true;
        });
        cm->addAction(ac);

        ac = new QAction(tr("Copy link url to clipboard"),NULL);
        connect(ac, &QAction::triggered, [linkUrl,cb]() {
            cb->setText(linkUrl.toString());
        });
        cm->addAction(ac);

        cm->addSeparator();
    }

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

        if (!gSet->ctxSearchEngines.isEmpty()) {
            cm->addSeparator();

            QStringList searchNames = gSet->ctxSearchEngines.keys();
            searchNames.sort(Qt::CaseInsensitive);
            foreach (const QString& name, searchNames) {
                QUrl url = gSet->createSearchUrl(sText,name);

                ac = new QAction(name,NULL);
                connect(ac, &QAction::triggered, [url,this](){
                    new CSnippetViewer(snv->parentWnd, url);
                });

                QUrl fiurl = url;
                fiurl.setFragment(QString());
                fiurl.setQuery(QString());
                fiurl.setPath("/favicon.ico");
                CFaviconLoader* fl = new CFaviconLoader(snv,fiurl);
                connect(fl,&CFaviconLoader::gotIcon,[ac](const QIcon& icon){
                    ac->setIcon(icon);
                });
                fl->queryStart(false);

                cm->addAction(ac);
            }
        }

        cm->addSeparator();

        ac = new QAction(QIcon(":/img/nepomuk"),tr("Local indexed search"),NULL);
        connect(ac, &QAction::triggered, [sText,this](){
            CSearchTab *bt = new CSearchTab(snv->parentWnd);
            bt->searchTerm(sText);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("edit-find"),tr("Search on page"),NULL);
        connect(ac, &QAction::triggered, [sText,this](){
            snv->searchPanel->show();
            snv->searchEdit->setEditText(sText);
            snv->fwdButton->click();
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("accessories-dictionary"),tr("Local dictionary"),NULL);
        connect(ac, &QAction::triggered, [sText](){
            gSet->showDictionaryWindow(sText);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("document-edit-verify"),tr("Light translator"),NULL);
        connect(ac, &QAction::triggered, [sText](){
            gSet->showLightTranslator(sText);
        });
        cm->addAction(ac);


        cm->addSeparator();
    }

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

    if (cb->mimeData(QClipboard::Clipboard)->hasText())
        cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::Paste));
    cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::SelectAll));
    cm->addSeparator();

    if (!linkUrl.isEmpty()) {
        ac = new QAction(QIcon::fromTheme("preferences-web-browser-adblock"),
                            tr("Add AdBlock rule for link url..."), NULL);
        connect(ac, &QAction::triggered, [linkUrl,this](){
            QString u = linkUrl.toString();
            bool ok;
            u = QInputDialog::getText(snv, tr("Add AdBlock rule"), tr("Filter template"),
                                      QLineEdit::Normal, u, &ok);
            if (ok)
                gSet->adblockAppend(u);
        });
        cm->addAction(ac);
        cm->addSeparator();
    }

    QMenu *ccm = cm->addMenu(QIcon::fromTheme("system-run"),tr("Service"));
    ccm->addAction(QIcon::fromTheme("document-edit-verify"),tr("Show source"),
                 this,SLOT(showSource()),QKeySequence(Qt::CTRL + Qt::Key_S));
    ccm->addSeparator();
    ccm->addAction(QIcon::fromTheme("documentation"),tr("Show in editor"),
                 this,SLOT(showInEditor()));

    ac = new QAction(QIcon::fromTheme("download"),tr("Open in browser"),NULL);
    ac->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
    connect(ac, &QAction::triggered,[this](){
        if (!QProcess::startDetached(gSet->settings.sysBrowser,
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start browser."));
    });
    ccm->addAction(ac);

    ac = new QAction(QIcon::fromTheme("system-run"),tr("Open with associated application"),NULL);
    connect(ac, &QAction::triggered,[this](){
        if (!QProcess::startDetached("xdg-open",
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start associated application."));
    });
    ccm->addAction(ac);

    ccm->addSeparator();

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

    menuActive->setInterval(1000);
    menuActive->start();

    connect(cm, &QMenu::aboutToHide, [this](){
        menuActive->stop();
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
        if (!text.isEmpty() && !menuActive->isActive()) {
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
        fname = getSaveFileNameD(snv,tr("Save to HTML"),gSet->settings.savedAuxSaveDir,
                                                 tr("HTML file (*.html);;Text file (*.txt)"));
    else
        fname = getSaveFileNameD(snv,tr("Save to file"),gSet->settings.savedAuxSaveDir,
                                                 tr("Text file (*.txt)"));

    if (fname.isNull() || fname.isEmpty()) return;
    gSet->settings.savedAuxSaveDir = QFileInfo(fname).absolutePath();

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

void CSnCtxHandler::showInEditor()
{
    QString fname = gSet->makeTmpFileName("html",true);
    snv->txtBrowser->page()->toHtml([fname,this](const QString& html) {
        QFile tfile(fname);
        tfile.open(QIODevice::WriteOnly);
        tfile.write(html.toUtf8());
        tfile.close();
        gSet->createdFiles.append(fname);

        if (!QProcess::startDetached(gSet->settings.sysEditor, QStringList() << fname))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start editor."));
    });
}

void CSnCtxHandler::showSource()
{
    CSourceViewer* srcv = new CSourceViewer(snv);
    srcv->show();
}
