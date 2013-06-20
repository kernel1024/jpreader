#include <QImageWriter>
#include <QMessageBox>
#include <QInputDialog>
#include <QProcess>

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QUrlQuery>
#endif

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
}

void CSnCtxHandler::contextMenu(const QPoint &pos)
{
    QWebHitTestResult wh = snv->txtBrowser->page()->mainFrame()->hitTestContent(pos);
    snv->lastFrame=wh.frame();

    QMenu cm(snv);
    if (!wh.linkUrl().isEmpty()) {
        QAction* nt = new QAction(QIcon::fromTheme("tab-new"),tr("Open in New Tab"),NULL);
        nt->setData(wh.linkUrl());
        connect(nt,SIGNAL(triggered()),this,SLOT(openNewTab()));
        cm.addAction(nt);
        cm.addAction(snv->txtBrowser->pageAction(QWebPage::CopyLinkToClipboard));
    }

    if (!snv->txtBrowser->selectedText().isEmpty()) {
        cm.addAction(snv->txtBrowser->pageAction(QWebPage::Copy));
        QAction* nt = new QAction(QIcon::fromTheme("document-edit-verify"),tr("Translate"),NULL);
        nt->setData(snv->txtBrowser->selectedText());
        connect(nt,SIGNAL(triggered()),this,SLOT(translateFragment()));
        cm.addAction(nt);
        nt = new QAction(QIcon::fromTheme("tab-new-background"),tr("Create plain text in separate tab"),NULL);
        nt->setData(snv->txtBrowser->selectedText());
        connect(nt,SIGNAL(triggered()),this,SLOT(createPlainTextTab()));
        cm.addAction(nt);
    }

    if (!wh.imageUrl().isEmpty()) {
        QByteArray ba;
        ba.clear();
        if (!wh.pixmap().isNull()) {
            QDataStream sba(&ba,QIODevice::WriteOnly);
            sba << wh.pixmap() << wh.imageUrl();
        }

        cm.addSeparator();
        QMenu* icm = cm.addMenu(QIcon::fromTheme("file-preview"),tr("Image"));

        icm->addAction(snv->txtBrowser->pageAction(QWebPage::CopyImageToClipboard));

        QAction* ac = icm->addAction(tr("Copy Image Url to Clipboard"),this,SLOT(copyImgUrl()));
        ac->setData(wh.imageUrl());

        icm->addSeparator();

        ac = icm->addAction(tr("Open Image in New Tab"),this,SLOT(openImgNewTab()));
        ac->setData(wh.imageUrl());

        if (!ba.isEmpty()) {
            ac = icm->addAction(tr("Save Image to File..."),this,SLOT(saveImgToFile()));
            ac->setData(ba);
        }
    }

    cm.addSeparator();
    cm.addAction(QIcon::fromTheme("go-previous"),tr("Back"),snv->msgHandler,SLOT(navBack()),QKeySequence(Qt::CTRL + Qt::Key_Z));
    cm.addAction(QIcon::fromTheme("view-refresh"),tr("Reload"),snv->netHandler,SLOT(reloadMedia()),QKeySequence(Qt::CTRL + Qt::Key_R));
    if (!snv->txtBrowser->selectedText().isEmpty()) {
        cm.addSeparator();
        QAction *ac = cm.addAction(QIcon(":/google"),tr("Search in Google"),this,SLOT(searchInGoogle()));
        ac->setData(snv->txtBrowser->selectedText());
        ac = cm.addAction(QIcon::fromTheme("nepomuk"),tr("Local search"),this,SLOT(searchLocal()));
        ac->setData(snv->txtBrowser->selectedText());
        ac = cm.addAction(QIcon(":/jisho"),tr("Jisho word translation"),this,SLOT(searchInJisho()));
        ac->setData(snv->txtBrowser->selectedText());
    }

    cm.addSeparator();
    cm.addAction(QIcon::fromTheme("tab-duplicate"),tr("Duplicate tab"),this,SLOT(duplicateTab()));
    cm.addAction(QIcon::fromTheme("tab-detach"),tr("Detach tab"),this,SLOT(detachTab()));
    if (snv->lastFrame->parentFrame()) {
        cm.addSeparator();
        cm.addAction(QIcon::fromTheme("document-import"),tr("Open current frame in tab"), this, SLOT(openFrame()));
        QAction* ac = cm.addAction(QIcon::fromTheme("document-export"),tr("Open current frame in tab background"), this, SLOT(openFrame()));
        ac->setData(QVariant(1234));
    }
    cm.addSeparator();
    cm.addAction(QIcon::fromTheme("bookmark-new"),tr("Add current frame to bookmarks"), this, SLOT(bookmarkFrame()));
    if (snv->lastFrame->parentFrame()) {
        QAction* ac = cm.addAction(QIcon::fromTheme("bookmark-new-list"),tr("Add parent frame to bookmarks"), this, SLOT(bookmarkFrame()));
        ac->setData(QVariant(1234));
    }
    cm.addSeparator();
    if ((!wh.imageUrl().isEmpty()) || (!wh.linkUrl().isEmpty())) {
        QMenu* acm = cm.addMenu(QIcon::fromTheme("emblem-important"),tr("AdBlock"));
        if (!wh.imageUrl().isEmpty()) {
            QAction* act = acm->addAction(tr("Add Block by Image Url..."),this,SLOT(addContextBlock()));
            act->setData(wh.imageUrl());
        }
        if (!wh.linkUrl().isEmpty()) {
            QAction* act = acm->addAction(tr("Add Block by Link Url..."),this,SLOT(addContextBlock()));
            act->setData(wh.linkUrl());
        }
        cm.addSeparator();
    }
    cm.addAction(QIcon::fromTheme("documentation"),tr("Show source"),this,SLOT(loadKwrite()),QKeySequence(Qt::CTRL + Qt::Key_S));
    cm.addAction(QIcon::fromTheme("download"),tr("Open in browser"),this,SLOT(execKonq()),QKeySequence(Qt::CTRL + Qt::Key_W));
    cm.addAction(snv->txtBrowser->pageAction(QWebPage::SelectAll));
    cm.addSeparator();
    QAction* atc = cm.addAction(QIcon::fromTheme("document-edit-decrypt"),tr("Automatic translation"),this,SLOT(autoTranslateMenu()));
    atc->setCheckable(true);
    atc->setChecked(gSet->autoTranslate);
    atc = cm.addAction(QIcon::fromTheme("character-set"),tr("Override font for translated text"),this,SLOT(overrideFontMenu()));
    atc->setCheckable(true);
    atc->setChecked(gSet->useOverrideFont);
    atc = cm.addAction(QIcon::fromTheme("view-preview"),tr("Autoload images"),this,SLOT(autoloadImagesMenu()));
    atc->setCheckable(true);
    atc->setChecked(QWebSettings::globalSettings()->testAttribute(QWebSettings::AutoLoadImages));
    atc = cm.addAction(QIcon::fromTheme("format-text-color"),tr("Force translated text color"),this,SLOT(overrideFontColorMenu()));
    atc->setCheckable(true);
    atc->setChecked(gSet->forceFontColor);
    cm.addSeparator();
    cm.addAction(QIcon::fromTheme("document-save"),tr("Save to file..."),this,SLOT(saveToFile()));
    cm.exec(snv->txtBrowser->mapToGlobal(pos));
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

void CSnCtxHandler::gotTranslation(const QString &text)
{
    if (!text.isEmpty()) {
        QSpecToolTipLabel* lbl = new QSpecToolTipLabel(wordWrap(text,80));
        connect(lbl,SIGNAL(labelHide()),this,SLOT(toolTipHide()));
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

void CSnCtxHandler::autoTranslateMenu()
{
    gSet->autoTranslate=!gSet->autoTranslate;
}

void CSnCtxHandler::overrideFontMenu()
{
    gSet->useOverrideFont=!gSet->useOverrideFont;
}

void CSnCtxHandler::overrideFontColorMenu()
{
    gSet->forceFontColor=!gSet->forceFontColor;
}

void CSnCtxHandler::autoloadImagesMenu()
{
    QWebSettings::globalSettings()->setAttribute(QWebSettings::AutoLoadImages,
                                                 !QWebSettings::globalSettings()->testAttribute(
                                                     QWebSettings::AutoLoadImages));
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

    if (snv->fileChanged) {
        sv->txtBrowser->setHtml(snv->txtBrowser->page()->mainFrame()->toHtml(),
                                snv->txtBrowser->page()->mainFrame()->baseUrl());
    }
}

void CSnCtxHandler::detachTab()
{
    CMainWindow* mwnd = gSet->addMainWindow(false,false);

    QUrl url;
    url.clear();
    if (!snv->fileChanged) url=snv->Uri;

    CSnippetViewer* sv = new CSnippetViewer(mwnd, url);

    if (snv->fileChanged) {
        sv->txtBrowser->setHtml(snv->txtBrowser->page()->mainFrame()->toHtml(),
                                snv->txtBrowser->page()->mainFrame()->baseUrl());
    }
    snv->closeTab(true);
}

void CSnCtxHandler::searchInGoogle()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();
    QUrl u("http://google.com/search");
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    u.addQueryItem("q", s);
#else
    QUrlQuery qu;
    qu.addQueryItem("q", s);
    u.setQuery(qu);
#endif

    new CSnippetViewer(snv->parentWnd, u);
}

void CSnCtxHandler::searchLocal()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();

    CSearchTab *bt = new CSearchTab(snv->parentWnd->tabMain,snv->parentWnd);
    bt->searchTerm(s);
}

void CSnCtxHandler::searchInJisho()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QString s = nt->data().toString();
    QUrl u("http://jisho.org/words");
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    u.addQueryItem("jap",s);
    u.addQueryItem("eng=","");
    u.addQueryItem("dict","edict");
#else
    QUrlQuery qu;
    qu.addQueryItem("jap",s);
    qu.addQueryItem("eng=","");
    qu.addQueryItem("dict","edict");
    u.setQuery(qu);
#endif

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
    QString fname = getSaveFileNameD(snv,tr("Save to HTML"),QDir::homePath(),
                                                 tr("HTML file (*.html);;Text file (*.txt)"));
    if (fname.isNull() || fname.isEmpty()) return;

    QFile f(fname);
    f.open(QIODevice::WriteOnly|QIODevice::Truncate);
    QFileInfo fi(fname);
    QByteArray bf;
    if (fi.suffix().toLower()=="txt") {
        bf = snv->txtBrowser->page()->mainFrame()->toPlainText().toUtf8();
    } else {
        //QTextCodec* cd = detectEncoding(snv->txtBrowser->page()->mainFrame()->toHtml().toAscii());
        //bf = cd->fromUnicode(snv->txtBrowser->page()->mainFrame()->toHtml());
        bf = snv->txtBrowser->page()->mainFrame()->toHtml().toUtf8();
    }
    f.write(bf);
    f.close();
}

void CSnCtxHandler::openNewTab()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==NULL) return;
    QUrl u = nt->data().toUrl();
    if (u.isRelative() && snv->lastFrame) {
        QUrl uu = snv->lastFrame->requestedUrl();
        if (uu.isEmpty() || uu.isRelative())
            uu = snv->Uri;
        u = uu.resolved(u);
    }
    u = snv->netHandler->fixUrl(u);

    new CSnippetViewer(snv->parentWnd, u, QStringList(), false, "", snv->comboZoom->currentText());
}

void CSnCtxHandler::openFrame() {
    if (!snv->lastFrame) return;
    QPointer<QWebFrame> lf = snv->lastFrame;
    if (!lf->requestedUrl().isValid()) return;
    QAction* ac = qobject_cast<QAction*>(sender());
    bool focused=true;
    if (ac!=NULL && ac->data().toInt()==1234) focused=false;

    new CSnippetViewer(snv->parentWnd,lf->requestedUrl(),QStringList(),focused, "", snv->comboZoom->currentText());
}

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

void CSnCtxHandler::bookmarkFrame() {
    if (!snv->lastFrame) return;
    QPointer<QWebFrame> lf = snv->lastFrame;
    QAction* ac = qobject_cast<QAction*>(sender());
    if (ac!=NULL && ac->data().toInt()==1234) lf=lf->parentFrame();
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
    QString fname = snv->Uri.toLocalFile();
    if (fname.isEmpty() || snv->fileChanged)
    {
        QString uuid = QUuid::createUuid().toString().remove(QRegExp("[^a-z,A-Z,0,1-9,-]"))+".html";
        fname = QDir::temp().absoluteFilePath(uuid);
        QFile tfile(fname);
        tfile.open(QIODevice::WriteOnly);
        tfile.write(snv->txtBrowser->page()->mainFrame()->toHtml().toUtf8());
        tfile.close();
        gSet->createdFiles.append(fname);
    }
    if (!QProcess::startDetached(gSet->sysEditor, QStringList() << fname))
        QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start editor."));
}

void CSnCtxHandler::execKonq()
{
    if (!QProcess::startDetached(gSet->sysBrowser, QStringList() << QString::fromUtf8(snv->Uri.toEncoded())))
        QMessageBox::critical(snv, trUtf8("JPReader"), trUtf8("Unable to start browser."));
}

