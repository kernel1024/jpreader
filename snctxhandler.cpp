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
#include "searchtab.h"
#include "specwidgets.h"
#include "auxtranslator.h"
#include "sourceviewer.h"
#include "bookmarks.h"
#include "authdlg.h"

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

void CSnCtxHandler::contextMenu(const QPoint &pos, const QWebEngineContextMenuData& data)
{
    QString sText;
    QUrl linkUrl, imageUrl;
    if (data.isValid()) {
        sText = data.selectedText();
        linkUrl = data.linkUrl();;
        if (data.mediaType()==QWebEngineContextMenuData::MediaTypeImage)
            imageUrl = data.mediaUrl();
    }

    QClipboard *cb = QApplication::clipboard();
    QAction *ac;
    QMenu *cm = new QMenu(snv);

    if (linkUrl.isValid()) {
        QUrl nUrl = linkUrl;
        if (checkAndUnpackUrl(nUrl)) {
            ac = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new background tab"),nullptr);
            connect(ac, &QAction::triggered, [nUrl,this]() {
                new CSnippetViewer(snv->parentWnd,nUrl,QStringList(),false);
            });
            cm->addAction(ac);
            ac = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new tab"),nullptr);
            connect(ac, &QAction::triggered, [nUrl,this]() {
                new CSnippetViewer(snv->parentWnd,nUrl,QStringList());
            });
            cm->addAction(ac);
        } else {
            cm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::OpenLinkInNewBackgroundTab));
            cm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::OpenLinkInNewTab));
        }

        ac = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new background tab and translate"),nullptr);
        connect(ac, &QAction::triggered, [linkUrl,this]() {
            CSnippetViewer* sn = new CSnippetViewer(snv->parentWnd,linkUrl,QStringList(),false);
            sn->requestAutotranslate = true;
        });
        cm->addAction(ac);

        cm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyLinkToClipboard));
        cm->addSeparator();
    }

    if ((linkUrl.isValid() && linkUrl.toString().contains(QString("pixiv.net/novel/show.php"), Qt::CaseInsensitive))
            || (!snv->fileChanged && snv->urlEdit->text().contains(QString("pixiv.net/novel/show.php"), Qt::CaseInsensitive))) {
        QUrl purl = QUrl::fromUserInput(snv->urlEdit->text());
        QString title = snv->txtBrowser->page()->title();
        if (linkUrl.isValid()) {
            purl = linkUrl;
            title.clear();
        }

        purl.setFragment(QString());
        ac = new QAction(tr("Extract pixiv novel in new background tab"),nullptr);
        connect(ac, &QAction::triggered, [this,purl,title](){
            snv->netHandler->processPixivNovel(purl,title,false,false);
        });

        QAction* ac2 = new QAction(tr("Extract pixiv novel in new tab"),nullptr);
        connect(ac2, &QAction::triggered, [this,purl,title](){
            snv->netHandler->processPixivNovel(purl,title,false,true);
        });

        QAction* ac3 = new QAction(tr("Extract pixiv novel in new background tab and translate"),nullptr);
                connect(ac3, &QAction::triggered, [this,purl,title](){
            snv->netHandler->processPixivNovel(purl,title,true,false);
        });

        QUrl fiurl("http://www.pixiv.net/favicon.ico");
        fiurl.setScheme(purl.scheme());
        CFaviconLoader* fl = new CFaviconLoader(snv,fiurl);
        connect(fl,&CFaviconLoader::gotIcon,[ac,ac2,ac3](const QIcon& icon){
            ac->setIcon(icon);
            ac2->setIcon(icon);
            ac3->setIcon(icon);
        });
        fl->queryStart(false);

        cm->addAction(ac);
        cm->addAction(ac2);
        cm->addAction(ac3);
        cm->addSeparator();
    }

    if (!sText.isEmpty()) {
        cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::Copy));

        ac = new QAction(QIcon::fromTheme("document-edit-verify"),tr("Translate"),nullptr);
        ac->setData(sText);
        connect(ac, &QAction::triggered,this, &CSnCtxHandler::translateFragment);
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("tab-new-background"),tr("Create plain text in separate tab"),nullptr);
        connect(ac, &QAction::triggered, [sText,this](){
            QString s = sText;
            s = s.replace('\n',"<br/>");
            new CSnippetViewer(snv->parentWnd,QUrl(),QStringList(),true,s);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("tab-new-background"),tr("Translate plain text in separate tab"),nullptr);
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

                ac = new QAction(name,nullptr);
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

        ac = new QAction(QIcon(":/img/nepomuk"),tr("Local indexed search"),nullptr);
        connect(ac, &QAction::triggered, [sText,this](){
            CSearchTab *bt = new CSearchTab(snv->parentWnd);
            bt->searchTerm(sText);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("edit-find"),tr("Search on page"),nullptr);
        connect(ac, &QAction::triggered, [sText,this](){
            snv->searchPanel->show();
            snv->searchEdit->setEditText(sText);
            snv->fwdButton->click();
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("accessories-dictionary"),tr("Local dictionary"),nullptr);
        connect(ac, &QAction::triggered, [sText](){
            gSet->showDictionaryWindowEx(sText);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme("document-edit-verify"),tr("Light translator"),nullptr);
        connect(ac, &QAction::triggered, [sText](){
            gSet->showLightTranslator(sText);
        });
        cm->addAction(ac);

        QUrl selectedUrl = QUrl::fromUserInput(sText);
        if (selectedUrl.isValid() && !selectedUrl.isLocalFile() && !selectedUrl.isRelative()) {
            cm->addSeparator();

            QMenu* icm = cm->addMenu(QIcon::fromTheme("go-jump-locationbar"),tr("Open selected link"));

            ac = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new background tab"),nullptr);
            connect(ac, &QAction::triggered, [selectedUrl,this]() {
                new CSnippetViewer(snv->parentWnd,selectedUrl,QStringList(),false);
            });
            icm->addAction(ac);
            ac = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new tab"),nullptr);
            connect(ac, &QAction::triggered, [selectedUrl,this]() {
                new CSnippetViewer(snv->parentWnd,selectedUrl,QStringList());
            });
            icm->addAction(ac);

            ac = new QAction(QIcon::fromTheme("tab-new"),tr("Open in new background tab and translate"),nullptr);
            connect(ac, &QAction::triggered, [selectedUrl,this]() {
                CSnippetViewer* sn = new CSnippetViewer(snv->parentWnd,selectedUrl,QStringList(),false);
                sn->requestAutotranslate = true;
            });
            icm->addAction(ac);
        }

        cm->addSeparator();
    }

    if (imageUrl.isValid()) {
        QMenu* icm = cm->addMenu(QIcon::fromTheme("view-preview"),tr("Image"));

        icm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyImageToClipboard));
        icm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyImageUrlToClipboard));
        icm->addSeparator();

        ac = icm->addAction(tr("Open image in new tab"));
        connect(ac,&QAction::triggered,[this,imageUrl](){
            new CSnippetViewer(snv->parentWnd,imageUrl);
        });
        icm->addAction(ac);

        icm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::DownloadImageToDisk));
        cm->addSeparator();
    }

    ac = new QAction(QIcon::fromTheme("tab-duplicate"),tr("Duplicate tab"),nullptr);
    connect(ac, &QAction::triggered, [this](){
        QString url("about://blank");
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
                            tr("Add AdBlock rule for link url..."), nullptr);
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

    QMenu *ccm = cm->addMenu(QIcon::fromTheme("dialog-password"),tr("Form autofill"));
    QUrl origin = snv->txtBrowser->page()->url();
    if (gSet->haveSavedPassword(origin)) {
        ac = ccm->addAction(QIcon::fromTheme("tools-wizard"),tr("Insert username and password"),
                            snv->msgHandler,SLOT(pastePassword()));
        ac->setData(1);
        ac = ccm->addAction(tr("Insert username"),snv->msgHandler,SLOT(pastePassword()));
        ac->setData(2);
        ac = ccm->addAction(tr("Insert password"),snv->msgHandler,SLOT(pastePassword()));
        ac->setData(3);
        ccm->addSeparator();
        ac = ccm->addAction(QIcon::fromTheme("edit-delete"),tr("Delete saved username and password"));
        connect(ac, &QAction::triggered, [origin](){
            gSet->removePassword(origin);
        });
    } else {
        ac = ccm->addAction(QIcon::fromTheme("edit-rename"),tr("Save username and password"));
        connect(ac, &QAction::triggered, [origin](){
            QString realm = gSet->cleanUrlForRealm(origin).toString();
            if (realm.length()>60) realm = QString("...%1").arg(realm.right(60));
            CAuthDlg *dlg = new CAuthDlg(QApplication::activeWindow(),origin,realm,true);
            dlg->exec();
            dlg->setParent(nullptr);
            delete dlg;
        });
    }
    cm->addSeparator();

    ccm = cm->addMenu(QIcon::fromTheme("system-run"),tr("Service"));
    ccm->addAction(QIcon::fromTheme("document-edit-verify"),tr("Show source"),
                 this,SLOT(showSource()),QKeySequence(Qt::CTRL + Qt::Key_E));
    ccm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::InspectElement));
    ccm->addSeparator();
    ccm->addAction(QIcon::fromTheme("documentation"),tr("Show in editor"),
                 this,SLOT(showInEditor()));

    ac = new QAction(QIcon::fromTheme("download"),tr("Open in browser"),nullptr);
    ac->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
    connect(ac, &QAction::triggered,[this](){
        if (!QProcess::startDetached(gSet->settings.sysBrowser,
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start browser."));
    });
    ccm->addAction(ac);

    ac = new QAction(QIcon::fromTheme("system-run"),tr("Open with associated application"),nullptr);
    connect(ac, &QAction::triggered,[this](){
        if (!QProcess::startDetached("xdg-open",
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start associated application."));
    });
    ccm->addAction(ac);

    ccm->addSeparator();

    ccm->addAction(QIcon::fromTheme("download-later"),tr("Download all images"),
                   snv->transHandler,SLOT(getImgUrlsAndParse()));
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

void CSnCtxHandler::reconfigureDefaultActions()
{
    QWebEnginePage* p = snv->txtBrowser->page();
    if (p==nullptr) return;

    p->action(QWebEnginePage::Paste)->setIcon(QIcon::fromTheme("edit-paste"));
    p->action(QWebEnginePage::SelectAll)->setIcon(QIcon::fromTheme("edit-select-all"));
    p->action(QWebEnginePage::OpenLinkInNewTab)->setIcon(QIcon::fromTheme("tab-new"));
    p->action(QWebEnginePage::OpenLinkInNewTab)->setText(tr("Open in new tab"));
    p->action(QWebEnginePage::OpenLinkInNewBackgroundTab)->setIcon(QIcon::fromTheme("tab-new"));
    p->action(QWebEnginePage::OpenLinkInNewBackgroundTab)->setText(tr("Open in new background tab"));
    p->action(QWebEnginePage::CopyLinkToClipboard)->setText(tr("Copy link url to clipboard"));
    p->action(QWebEnginePage::CopyImageToClipboard)->setText(tr("Copy image to clipboard"));
    p->action(QWebEnginePage::CopyImageUrlToClipboard)->setText(tr("Copy image url to clipboard"));
    p->action(QWebEnginePage::DownloadImageToDisk)->setText(tr("Save image to file..."));
}

void CSnCtxHandler::translateFragment()
{
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt==nullptr) return;
    QString s = nt->data().toString();
    if (s.isEmpty()) return;
    CLangPair lp = gSet->ui.getActiveLangPair();
    if (!lp.isValid()) return;

    QThread *th = new QThread();
    CAuxTranslator *at = new CAuxTranslator();
    at->setText(s);
    at->setSrcLang(lp.langFrom.bcp47Name());
    at->setDestLang(lp.langTo.bcp47Name());
    connect(this,&CSnCtxHandler::startTranslation,
            at,&CAuxTranslator::startTranslation,Qt::QueuedConnection);

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

    emit startTranslation(true);
}

void CSnCtxHandler::saveToFile()
{
    QString selectedText;
    selectedText.clear();
    QAction* nt = qobject_cast<QAction *>(sender());
    if (nt!=nullptr)
        selectedText = nt->data().toString();

    QString fname;
#if QT_VERSION >= 0x050800
    if (selectedText.isEmpty())
        fname = getSaveFileNameD(snv,tr("Save to HTML"),gSet->settings.savedAuxSaveDir,
                                                 tr("HTML file (*.htm);;Complete HTML with files (*.html);;"
                                                    "Text file (*.txt);;MHTML archive (*.mht)"));
    else
#endif
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
    } else if (fi.suffix().toLower().startsWith("txt"))
        snv->txtBrowser->page()->toPlainText([fname](const QString& result)
        {
            QFile f(fname);
            f.open(QIODevice::WriteOnly|QIODevice::Truncate);
            f.write(result.toUtf8());
            f.close();
        });
#if QT_VERSION >= 0x050800
    else if (fi.suffix().toLower().startsWith("mht"))
        snv->txtBrowser->page()->save(fname,QWebEngineDownloadItem::MimeHtmlSaveFormat);
    else if (fi.suffix().toLower()==QLatin1String("htm"))
        snv->txtBrowser->page()->save(fname,QWebEngineDownloadItem::SingleHtmlSaveFormat);
    else
        snv->txtBrowser->page()->save(fname,QWebEngineDownloadItem::CompleteHtmlSaveFormat);
#endif
}

void CSnCtxHandler::bookmarkPage() {
    QWebEnginePage *lf = snv->txtBrowser->page();
    AddBookmarkDialog *dlg = new AddBookmarkDialog(lf->requestedUrl().toString(),lf->title(),snv);
    if (dlg->exec())
        emit gSet->updateAllBookmarks();
    dlg->setParent(nullptr);
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
