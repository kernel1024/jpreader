#include <QImageWriter>
#include <QMessageBox>
#include <QInputDialog>
#include <QProcess>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QWebEnginePage>

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
    connect(&menuActive,SIGNAL(timeout()),this,SLOT(menuOpened()));
}

void CSnCtxHandler::contextMenu(const QPoint &pos)
{
    QUrl wUrl = QUrl(); // unable to get url on link click

    QString sText = snv->txtBrowser->selectedText();

    QMenu *cm = new QMenu(snv);
    if (!wUrl.isEmpty()) {
        QAction* nt = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new tab"),NULL);
        nt->setData(wUrl);
        connect(nt,SIGNAL(triggered()),this,SLOT(openNewTab()));
        cm->addAction(nt);
        nt = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new tab and translate"),NULL);
        nt->setData(wUrl);
        connect(nt,SIGNAL(triggered()),this,SLOT(openNewTabTranslate()));
        cm->addAction(nt);
    }

/*    QString tx = wh.element().toPlainText();
    if (tx.isEmpty())
        tx = wh.enclosingBlockElement().toPlainText();
    if (!tx.isEmpty()) {
        cm->addSeparator();
        QAction* nta = new QAction(tr("Open block text in separate tab"),NULL);
        nta->setData(tx);
        connect(nta,SIGNAL(triggered()),this,SLOT(openFragment()));
        cm->addAction(nta);
        nta = new QAction(QIcon::fromTheme("text-frame-link"),tr("Translate block"),NULL);
        nta->setData(tx);
        connect(nta,SIGNAL(triggered()),this,SLOT(translateFragment()));
        cm->addAction(nta);
    }*/

    if (!sText.isEmpty()) {
        cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::Copy));

        QAction* nt = new QAction(QIcon::fromTheme("document-edit-verify"),tr("Translate"),NULL);
        nt->setData(sText);
        connect(nt,SIGNAL(triggered()),this,SLOT(translateFragment()));
        cm->addAction(nt);

        nt = new QAction(QIcon::fromTheme("tab-new-background"),tr("Create plain text in separate tab"),NULL);
        nt->setData(sText);
        connect(nt,SIGNAL(triggered()),this,SLOT(createPlainTextTab()));
        cm->addAction(nt);

        nt = new QAction(QIcon::fromTheme("tab-new-background"),tr("Translate plain text in separate tab"),NULL);
        nt->setData(sText);
        connect(nt,SIGNAL(triggered()),this,SLOT(createPlainTextTabTranslate()));
        cm->addAction(nt);
    }

/*    if (!wh.imageUrl().isEmpty()) {
        QByteArray ba;
        ba.clear();
        if (!wh.pixmap().isNull()) {
            QDataStream sba(&ba,QIODevice::WriteOnly);
            sba << wh.pixmap() << wh.imageUrl();
        }

        cm->addSeparator();
        QMenu* icm = cm->addMenu(QIcon::fromTheme("file-preview"),tr("Image"));

        icm->addAction(snv->txtBrowser->pageAction(QWebPage::CopyImageToClipboard));

        QAction* ac = icm->addAction(tr("Copy image url to clipboard"),this,SLOT(copyImgUrl()));
        ac->setData(wh.imageUrl());

        icm->addSeparator();

        ac = icm->addAction(tr("Open image in new tab"),this,SLOT(openImgNewTab()));
        ac->setData(wh.imageUrl());

        if (!ba.isEmpty()) {
            ac = icm->addAction(tr("Save image to file..."),this,SLOT(saveImgToFile()));
            ac->setData(ba);
        }
    }*/

    if (!sText.isEmpty()) {
        cm->addSeparator();
        QAction *ac = cm->addAction(QIcon(":/img/google"),tr("Search in Google"),
                                   this,SLOT(searchInGoogle()));
        ac->setData(sText);
        ac = cm->addAction(QIcon::fromTheme(":/img/nepomuk"),tr("Local search"),
                          this,SLOT(searchLocal()));
        ac->setData(sText);
        ac = cm->addAction(QIcon(":/img/jisho"),tr("Jisho word translation"),
                          this,SLOT(searchInJisho()));
        ac->setData(sText);
        ac = cm->addAction(QIcon::fromTheme("accessories-dictionary"),tr("Local dictionary"),
                          this,SLOT(showDictionary()));
        ac->setData(sText);
    }

    cm->addSeparator();
    cm->addAction(QIcon::fromTheme("tab-duplicate"),tr("Duplicate tab"),
                 this,SLOT(duplicateTab()));
    cm->addAction(QIcon::fromTheme("tab-detach"),tr("Detach tab"),
                 snv,SLOT(detachTab()));
    if (!wUrl.isEmpty()) {
        QAction *ac = cm->addAction(QIcon::fromTheme("window-new"),tr("Open in new window"),
                     this,SLOT(openNewWindow()));
        ac->setData(wUrl);
    }

    /*if (snv->lastFrame->parentFrame()) {
        cm->addSeparator();
        cm->addAction(QIcon::fromTheme("document-import"),tr("Open current frame in tab"),
                     this, SLOT(openFrame()));
        QAction* ac = cm->addAction(QIcon::fromTheme("document-export"),tr("Open current frame in tab background"),
                                   this, SLOT(openFrame()));
        ac->setData(QVariant(1234));
    }*/

    cm->addSeparator();
    cm->addAction(QIcon::fromTheme("bookmark-new"),tr("Add current page to bookmarks"),
                 this, SLOT(bookmarkPage()));

    cm->addSeparator();
    cm->addAction(QIcon::fromTheme("go-previous"),tr("Back"),
                 snv->txtBrowser,SLOT(back()),QKeySequence(Qt::CTRL + Qt::Key_Z));
    cm->addAction(QIcon::fromTheme("view-refresh"),tr("Reload"),
                 snv->txtBrowser,SLOT(reload()),QKeySequence(Qt::CTRL + Qt::Key_R));

    cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::SelectAll));
    cm->addSeparator();
//    if ((!wh.imageUrl().isEmpty()) || (!wUrl.isEmpty())) {
    if (!wUrl.isEmpty()) {
        QMenu* acm = cm->addMenu(QIcon::fromTheme("emblem-important"),tr("AdBlock"));
/*        if (!wh.imageUrl().isEmpty()) {
            QAction* act = acm->addAction(tr("Add block by image url..."),
                                          this,SLOT(addContextBlock()));
            act->setData(wh.imageUrl());
        }*/
        if (!wUrl.isEmpty()) {
            QAction* act = acm->addAction(tr("Add block by link url..."),
                                          this,SLOT(addContextBlock()));
            act->setData(wUrl);
        }
        cm->addSeparator();
    }

    QMenu *ccm = cm->addMenu(QIcon::fromTheme("system-run"),tr("Service"));
    ccm->addAction(QIcon::fromTheme("documentation"),tr("Show source"),
                 this,SLOT(loadKwrite()),QKeySequence(Qt::CTRL + Qt::Key_S));
    ccm->addAction(QIcon::fromTheme("download"),tr("Open in browser"),
                 this,SLOT(execKonq()),QKeySequence(Qt::CTRL + Qt::Key_W));
    ccm->addAction(QIcon::fromTheme("document-edit-decrypt"),tr("Convert to XML"),
                   snv->transHandler,SLOT(convertToXML()));
    ccm->addSeparator();
    ccm->addAction(QIcon::fromTheme("dialog-close"),tr("Close tab"),
                 snv,SLOT(closeTab()));
    ccm->addSeparator();
    if (!sText.isEmpty()) {
        QAction *act = ccm->addAction(QIcon::fromTheme("document-save-as"),tr("Save selected text as file..."),
                                      this,SLOT(saveToFile()));
        act->setData(sText);
    }
    ccm->addAction(QIcon::fromTheme("document-save"),tr("Save to file..."),
                 this,SLOT(saveToFile()));

    menuActive.setInterval(1000);
    menuActive.start();
    connect(cm,SIGNAL(aboutToHide()),this,SLOT(menuClosed()));
    cm->setAttribute(Qt::WA_DeleteOnClose,true);
    cm->popup(snv->txtBrowser->mapToGlobal(pos));
}

void CSnCtxHandler::menuClosed()
{
    menuActive.stop();
}

void CSnCtxHandler::menuOpened()
{
    emit hideTooltips();
}

void CSnCtxHandler::showDictionary()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();

    gSet->showDictionaryWindow(s);
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
    connect(at,SIGNAL(gotTranslation(QString)),this,SLOT(gotTranslation(QString)),Qt::QueuedConnection);
    at->moveToThread(th);
    th->start();

    emit startTranslation();
}

void CSnCtxHandler::openFragment()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();
    if (s.isEmpty()) return;
    s = s.replace('\n',"<br/>");

    new CSnippetViewer(snv->parentWnd,QUrl(),QStringList(),true,s);
}

void CSnCtxHandler::gotTranslation(const QString &text)
{
    if (!text.isEmpty() && !menuActive.isActive()) {
        QSpecToolTipLabel* lbl = new QSpecToolTipLabel(wordWrap(text,80));
        connect(lbl,SIGNAL(labelHide()),this,SLOT(toolTipHide()));
        connect(this,SIGNAL(hideTooltips()),lbl,SLOT(close()));
        QxtToolTip::show(QCursor::pos(),lbl,snv->parentWnd);
    }
}

void CSnCtxHandler::createPlainTextTab()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();
    if (s.isEmpty()) return;
    s = s.replace('\n',"<br/>");

    new CSnippetViewer(snv->parentWnd,QUrl(),QStringList(),true,s);
}

void CSnCtxHandler::createPlainTextTabTranslate()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();
    if (s.isEmpty()) return;
    s = s.replace('\n',"<br/>");

    CSnippetViewer *sn = new CSnippetViewer(snv->parentWnd,QUrl(),QStringList(),true,s);
    sn->transButton->click();
}

void CSnCtxHandler::copyImgUrl()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QApplication::clipboard()->setText(nt->data().toString(),QClipboard::Clipboard);
}

void CSnCtxHandler::duplicateTab()
{
    QString url = "about:blank";
    if (!snv->fileChanged) url=snv->urlEdit->currentText();

    CSnippetViewer* sv = new CSnippetViewer(snv->parentWnd, url);

/*    if (snv->fileChanged) {
        QUrl b = snv->txtBrowser->page()->url();
        //sv->netHandler->addUrlToProcessing(b);
        sv->txtBrowser->setHtml(snv->txtBrowser->page()->mainFrame()->toHtml(), b);
        //sv->netHandler->removeUrlFromProcessing(b);
    }*/
}

void CSnCtxHandler::openNewWindow()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;

    CMainWindow* mwnd = gSet->addMainWindow(false,false);

    QUrl u = nt->data().toUrl();
    if (u.isRelative()) {
        QUrl uu = snv->txtBrowser->page()->requestedUrl();
        if (uu.isEmpty() || uu.isRelative())
            uu = snv->getUrl();
        u = uu.resolved(u);
    }
//    u = snv->netHandler->fixUrl(u);

    new CSnippetViewer(mwnd, u);
}

void CSnCtxHandler::searchInGoogle()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();
    QUrl u("http://google.com/search");
    QUrlQuery qu;
    qu.addQueryItem("q", s);
    u.setQuery(qu);

    new CSnippetViewer(snv->parentWnd, u);
}

void CSnCtxHandler::searchLocal()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();

    CSearchTab *bt = new CSearchTab(snv->parentWnd);
    bt->searchTerm(s);
}

void CSnCtxHandler::searchInJisho()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();
    QUrl u("http://jisho.org/words");
    QUrlQuery qu;
    qu.addQueryItem("jap",s);
    qu.addQueryItem("eng=","");
    qu.addQueryItem("dict","edict");
    u.setQuery(qu);

    new CSnippetViewer(snv->parentWnd, u);
}

void CSnCtxHandler::toolTipHide()
{
    QxtToolTip::setToolTip(snv->parentWnd,NULL);
}

void CSnCtxHandler::openImgNewTab()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QUrl url = nt->data().toUrl();

    new CSnippetViewer(snv->parentWnd, url.toString(), QStringList(),true, QString(), snv->comboZoom->currentText());
}

void CSnCtxHandler::saveImgToFile()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;

    QByteArray ba = nt->data().toByteArray();
    QDataStream sba(&ba,QIODevice::ReadOnly);
    QPixmap px; QUrl imgurl;
    sba >> px >> imgurl;
    if (!px.isNull()) {
        QList<QByteArray> fmts = QImageWriter::supportedImageFormats();
        QString fltr = "";
        QStringList fmtss;
        for (int i=0;i<fmts.count();i++) {
            QString tmp = QString::fromLatin1(fmts.at(i));
            fmtss << tmp;
            if (fltr.isEmpty())
                fltr = QString(tr("%1 image (*.%1)")).arg(tmp);
            else
                fltr+= QString(tr(";;%1 image (*.%1)")).arg(tmp);
        }
        QString fname = QDir::homePath();
        if (!imgurl.isEmpty()) {
            QFileInfo fi(imgurl.path());
            fname+=QDir::separator()+fi.baseName();
            if (!fi.completeSuffix().isEmpty())
                fname+="."+fi.completeSuffix();
        }
        fname = getSaveFileNameD(snv,tr("Save image as"),fname,fltr);
        if (!fname.isEmpty()) {
            QFileInfo fi(fname);
            if (fmtss.contains(fi.suffix(),Qt::CaseInsensitive))
                px.save(fname,fi.suffix().toUtf8().data(),100);
            else
                QMessageBox::warning(snv,tr("JPReader"),tr("Wrong format selected (%1)").arg(fi.suffix()));
        }
    }
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

void CSnCtxHandler::openNewTab()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QUrl u = nt->data().toUrl();
    if (u.isRelative()) {
        QUrl uu = snv->txtBrowser->page()->requestedUrl();
        if (uu.isEmpty() || uu.isRelative())
            uu = snv->getUrl();
        u = uu.resolved(u);
    }
//    u = snv->netHandler->fixUrl(u);

    new CSnippetViewer(snv->parentWnd, u, QStringList(), false, "", snv->comboZoom->currentText());
}

void CSnCtxHandler::openNewTabTranslate()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QUrl u = nt->data().toUrl();
    if (u.isRelative()) {
        QUrl uu = snv->txtBrowser->page()->requestedUrl();
        if (uu.isEmpty() || uu.isRelative())
            uu = snv->getUrl();
        u = uu.resolved(u);
    }
//    u = snv->netHandler->fixUrl(u);

    CSnippetViewer* s = new CSnippetViewer(snv->parentWnd, u, QStringList(),
                                             false, "", snv->comboZoom->currentText());
    s->requestAutotranslate = true;
}

/*void CSnCtxHandler::openFrame() {
    if (!snv->lastFrame) return;
    QPointer<QWebFrame> lf = snv->lastFrame;
    if (!lf->requestedUrl().isValid()) return;
    QAction* ac = qobject_cast<QAction*>(sender());
    bool focused=true;
    if (ac!=NULL && ac->data().toInt()==1234) focused=false;

    new CSnippetViewer(snv->parentWnd,lf->requestedUrl(),QStringList(),focused, "", snv->comboZoom->currentText());
}*/

void CSnCtxHandler::addContextBlock()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QUrl url = nt->data().toUrl();
    if (url.isEmpty() || !url.isValid()) return;
    QString u = url.toString();
    bool ok;
    u = QInputDialog::getText(snv,tr("Add adblock filter"),tr("Filter template"),QLineEdit::Normal,u,&ok);
    if (ok) gSet->adblock.append(u);
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
        snv->txtBrowser->page()->toHtml([fname](const QString& html) {
            QFile tfile(fname);
            tfile.open(QIODevice::WriteOnly);
            tfile.write(html.toUtf8());
            tfile.close();
            gSet->createdFiles.append(fname);
        });
    }
    if (!QProcess::startDetached(gSet->sysEditor, QStringList() << fname))
        QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start editor."));
}

void CSnCtxHandler::execKonq()
{
    if (!QProcess::startDetached(gSet->sysBrowser, QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
        QMessageBox::critical(snv, trUtf8("JPReader"), trUtf8("Unable to start browser."));
}

