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
#include <QThread>

#include "qxttooltip.h"
#include "snctxhandler.h"
#include "genericfuncs.h"
#include "searchtab.h"
#include "specwidgets.h"
#include "auxtranslator.h"
#include "sourceviewer.h"
#include "bookmarks.h"
#include "authdlg.h"
#include "snnet.h"
#include "snmsghandler.h"
#include "sntrans.h"
#include "noscriptdialog.h"

const int menuActiveTimerInterval = 1000;
const int maxRealmLabelEliding = 60;
const int maxTranslateFragmentCharWidth = 80;

CSnCtxHandler::CSnCtxHandler(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    m_menuActive = new QTimer(this);
    m_menuActive->setSingleShot(false);
    m_menuActive->setInterval(menuActiveTimerInterval);

    connect(m_menuActive, &QTimer::timeout, this, [this](){
        Q_EMIT hideTooltips();
    });

    connect(snv->txtBrowser,&CSpecWebView::contextMenuRequested,this,&CSnCtxHandler::contextMenu);
}

void CSnCtxHandler::contextMenu(const QPoint &pos, const QWebEngineContextMenuData& data)
{
    QString sText;
    QUrl linkUrl;
    QUrl imageUrl;
    QUrl origin = snv->txtBrowser->page()->url();

    if (data.isValid()) {
        sText = data.selectedText();
        linkUrl = data.linkUrl();
        if (data.mediaType()==QWebEngineContextMenuData::MediaTypeImage)
            imageUrl = data.mediaUrl();
    }

    QClipboard *cb = QApplication::clipboard();
    QAction *ac;
    QMenu *ccm;
    auto cm = new QMenu(snv);

    if (linkUrl.isValid()) {
        QUrl nUrl = linkUrl;
        if (checkAndUnpackUrl(nUrl)) {
            ac = new QAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                                              tr("Open in new background tab"),nullptr);
            connect(ac, &QAction::triggered, this, [nUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),nUrl,QStringList(),false);
            });
            cm->addAction(ac);
            ac = new QAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                             tr("Open in new tab"),nullptr);
            connect(ac, &QAction::triggered, this, [nUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),nUrl,QStringList());
            });
            cm->addAction(ac);
        } else {
            cm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::OpenLinkInNewBackgroundTab));
            cm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::OpenLinkInNewTab));
        }

        ac = new QAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                         tr("Open in new background tab and translate"),nullptr);
        connect(ac, &QAction::triggered, this, [linkUrl,this]() {
            auto sn = new CSnippetViewer(snv->parentWnd(),linkUrl,QStringList(),false);
            sn->m_requestAutotranslate = true;
        });
        cm->addAction(ac);

        cm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyLinkToClipboard));
        cm->addSeparator();
    }

    if ((linkUrl.isValid() && linkUrl.toString().contains(
             QStringLiteral("pixiv.net/novel/show.php"), Qt::CaseInsensitive))
            || (!snv->m_fileChanged && snv->urlEdit->text().contains(
                    QStringLiteral("pixiv.net/novel/show.php"), Qt::CaseInsensitive))) {
        QUrl purl = QUrl::fromUserInput(snv->urlEdit->text());
        QString title = snv->txtBrowser->page()->title();
        if (linkUrl.isValid()) {
            purl = linkUrl;
            title.clear();
        }

        purl.setFragment(QString());
        ac = new QAction(tr("Extract pixiv novel in new background tab"),nullptr);
        connect(ac, &QAction::triggered, this, [this,purl,title](){
            snv->netHandler->processPixivNovel(purl,title,false,false);
        });

        QAction* ac2 = new QAction(tr("Extract pixiv novel in new tab"),nullptr);
        connect(ac2, &QAction::triggered, this, [this,purl,title](){
            snv->netHandler->processPixivNovel(purl,title,false,true);
        });

        QAction* ac3 = new QAction(tr("Extract pixiv novel in new background tab and translate"),nullptr);
                connect(ac3, &QAction::triggered, this, [this,purl,title](){
            snv->netHandler->processPixivNovel(purl,title,true,false);
        });

        QUrl fiurl(QStringLiteral("http://www.pixiv.net/favicon.ico"));
        fiurl.setScheme(purl.scheme());
        auto fl = new CFaviconLoader(snv,fiurl);
        connect(fl,&CFaviconLoader::gotIcon, ac, [ac,ac2,ac3](const QIcon& icon){
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

        ac = new QAction(QIcon::fromTheme(QStringLiteral("document-edit-verify")),
                         tr("Translate"),nullptr);
        ac->setData(sText);
        connect(ac, &QAction::triggered,this, &CSnCtxHandler::translateFragment);
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme(QStringLiteral("tab-new-background")),
                         tr("Create plain text in separate tab"),nullptr);
        connect(ac, &QAction::triggered, this, [sText,this](){
            QString s = sText;
            s = s.replace('\n',QStringLiteral("<br/>"));
            new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),true,s);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme(QStringLiteral("tab-new-background")),
                         tr("Translate plain text in separate tab"),nullptr);
        connect(ac, &QAction::triggered, this, [sText,this](){
            QString s = sText;
            s = s.replace('\n',QStringLiteral("<br/>"));
            CSnippetViewer* sn = new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),true,s);
            sn->m_requestAutotranslate = true;
        });
        cm->addAction(ac);

        QStringList searchNames = gSet->getSearchEngines();
        if (!searchNames.isEmpty()) {
            cm->addSeparator();

            searchNames.sort(Qt::CaseInsensitive);
            for (const QString& name : qAsConst(searchNames)) {
                QUrl url = gSet->createSearchUrl(sText,name);

                ac = new QAction(name,nullptr);
                connect(ac, &QAction::triggered, this, [url,this](){
                    new CSnippetViewer(snv->parentWnd(), url);
                });

                QUrl fiurl = url;
                fiurl.setFragment(QString());
                fiurl.setQuery(QString());
                fiurl.setPath(QStringLiteral("/favicon.ico"));
                auto fl = new CFaviconLoader(snv,fiurl);
                connect(fl,&CFaviconLoader::gotIcon,ac,[ac](const QIcon& icon){
                    ac->setIcon(icon);
                });
                fl->queryStart(false);

                cm->addAction(ac);
            }
        }

        cm->addSeparator();

        ac = new QAction(QIcon(QStringLiteral(":/img/nepomuk")),
                         tr("Local indexed search"),nullptr);
        connect(ac, &QAction::triggered, this, [sText,this](){
            auto bt = new CSearchTab(snv->parentWnd());
            bt->searchTerm(sText);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme(QStringLiteral("edit-find")),
                         tr("Search on page"),nullptr);
        connect(ac, &QAction::triggered, this, [sText,this](){
            snv->searchPanel->show();
            snv->searchEdit->setEditText(sText);
            snv->fwdButton->click();
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme(QStringLiteral("accessories-dictionary")),
                         tr("Local dictionary"),nullptr);
        connect(ac, &QAction::triggered, this, [sText](){
            gSet->showDictionaryWindowEx(sText);
        });
        cm->addAction(ac);

        ac = new QAction(QIcon::fromTheme(QStringLiteral("document-edit-verify")),
                         tr("Light translator"),nullptr);
        connect(ac, &QAction::triggered, this, [sText](){
            gSet->showLightTranslator(sText);
        });
        cm->addAction(ac);

        QUrl selectedUrl = QUrl::fromUserInput(sText);
        if (selectedUrl.isValid() && !selectedUrl.isLocalFile() && !selectedUrl.isRelative()) {
            cm->addSeparator();

            QMenu* icm = cm->addMenu(QIcon::fromTheme(QStringLiteral("go-jump-locationbar")),
                                     tr("Open selected link"));

            ac = new QAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                             tr("Open in new background tab"),nullptr);
            connect(ac, &QAction::triggered, this, [selectedUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),selectedUrl,QStringList(),false);
            });
            icm->addAction(ac);
            ac = new QAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                             tr("Open in new tab"),nullptr);
            connect(ac, &QAction::triggered, this, [selectedUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),selectedUrl,QStringList());
            });
            icm->addAction(ac);

            ac = new QAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                             tr("Open in new background tab and translate"),nullptr);
            connect(ac, &QAction::triggered, this, [selectedUrl,this]() {
                CSnippetViewer* sn = new CSnippetViewer(snv->parentWnd(),selectedUrl,QStringList(),false);
                sn->m_requestAutotranslate = true;
            });
            icm->addAction(ac);
        }

        cm->addSeparator();
    }

    if (imageUrl.isValid()) {
        QMenu* icm = cm->addMenu(QIcon::fromTheme(QStringLiteral("view-preview")),
                                 tr("Image"));

        icm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyImageToClipboard));
        icm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyImageUrlToClipboard));
        icm->addSeparator();

        ac = icm->addAction(tr("Open image in new tab"));
        connect(ac,&QAction::triggered,this,[this,imageUrl](){
            new CSnippetViewer(snv->parentWnd(),imageUrl);
        });
        icm->addAction(ac);

        icm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::DownloadImageToDisk));
        cm->addSeparator();
    }

    ac = new QAction(QIcon::fromTheme(QStringLiteral("tab-duplicate")),
                     tr("Duplicate tab"),nullptr);
    connect(ac, &QAction::triggered, this, [this](){
        QString url(QStringLiteral("about://blank"));
        if (!snv->m_fileChanged) url=snv->urlEdit->text();

        CSnippetViewer* sv = new CSnippetViewer(snv->parentWnd(), url);

        if (snv->m_fileChanged || url.isEmpty()) {
            snv->txtBrowser->page()->toHtml([sv,this](const QString& html) {
                if (!html.isEmpty())
                    sv->txtBrowser->setHtml(html,snv->txtBrowser->page()->url());
            });
        }
    });
    cm->addAction(ac);

    cm->addAction(QIcon::fromTheme(QStringLiteral("tab-detach")),tr("Detach tab"),
                 snv,&CSnippetViewer::detachTab);

    cm->addSeparator();
    cm->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")),
                  tr("Add current page to bookmarks"),
                  this, &CSnCtxHandler::bookmarkPage);

    cm->addSeparator();
    cm->addAction(QIcon::fromTheme(QStringLiteral("go-previous")),tr("Back"),
                 snv->txtBrowser,&CSpecWebView::back,QKeySequence(Qt::CTRL + Qt::Key_Z));
    cm->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),tr("Reload"),
                 snv->txtBrowser,&CSpecWebView::reload,QKeySequence(Qt::CTRL + Qt::Key_R));

    if (cb->mimeData(QClipboard::Clipboard)->hasText())
        cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::Paste));
    cm->addAction(snv->txtBrowser->page()->action(QWebEnginePage::SelectAll));
    cm->addSeparator();

    const QVector<CUserScript> scripts = gSet->getUserScriptsForUrl(origin,false,true);
    if (!scripts.isEmpty()) {
        ccm = cm->addMenu(QIcon::fromTheme(QStringLiteral("run-build")),
                          tr("Start user script"));
        for (const auto& script : scripts) {
            ac = ccm->addAction(script.getTitle());
            ac->setData(script.getSource());
        }
        connect(ac,&QAction::triggered,this,&CSnCtxHandler::runJavaScript);
    }
    if (gSet->settings()->useNoScript) {
        ac = new QAction(QIcon::fromTheme(QStringLiteral("dialog-cancel")),
                            tr("Enable scripts for this page..."), nullptr);
        connect(ac, &QAction::triggered, this, [this](){
            const QUrl origin = snv->getUrl();
            if (origin.isLocalFile() || origin.isRelative()) return;
            QString u = snv->getUrl().toString(CSettings::adblockUrlFmt);
            auto dlg = new CNoScriptDialog(snv,u);
            dlg->exec();
            dlg->setParent(nullptr);
            delete dlg;
        });
        cm->addAction(ac);
    }
    if (!scripts.isEmpty() || gSet->settings()->useNoScript)
        cm->addSeparator();

    if (!linkUrl.isEmpty()) {
        ac = new QAction(QIcon::fromTheme(QStringLiteral("preferences-web-browser-adblock")),
                            tr("Add AdBlock rule for link url..."), nullptr);
        connect(ac, &QAction::triggered, this, [linkUrl,this](){
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

    ccm = cm->addMenu(QIcon::fromTheme(QStringLiteral("dialog-password")),
                             tr("Form autofill"));
    if (gSet->haveSavedPassword(origin)) {
        ac = ccm->addAction(QIcon::fromTheme(QStringLiteral("tools-wizard")),
                            tr("Insert username and password"),
                            snv->msgHandler,&CSnMsgHandler::pastePassword);
        ac->setData(1);
        ac = ccm->addAction(tr("Insert username"),snv->msgHandler,&CSnMsgHandler::pastePassword);
        ac->setData(2);
        ac = ccm->addAction(tr("Insert password"),snv->msgHandler,&CSnMsgHandler::pastePassword);
        ac->setData(3);
        ccm->addSeparator();
        ac = ccm->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                            tr("Delete saved username and password"));
        connect(ac, &QAction::triggered, this, [origin](){
            gSet->removePassword(origin);
        });
    } else {
        ac = ccm->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                            tr("Save username and password"));
        connect(ac, &QAction::triggered, this, [origin](){
            QString realm = elideString(gSet->cleanUrlForRealm(origin).toString(),
                                        maxRealmLabelEliding,Qt::ElideLeft);
            auto dlg = new CAuthDlg(QApplication::activeWindow(),origin,realm,true);
            dlg->exec();
            dlg->setParent(nullptr);
            delete dlg;
        });
    }
    cm->addSeparator();

    ccm = cm->addMenu(QIcon::fromTheme(QStringLiteral("system-run")),tr("Service"));
    ccm->addAction(QIcon::fromTheme(QStringLiteral("document-edit-verify")),
                   tr("Show source"),
                   this,&CSnCtxHandler::showSource,QKeySequence(Qt::CTRL + Qt::Key_E));

#if QT_VERSION >= 0x050b00
    ac = ccm->addAction(tr("Inspect page"));
    connect(ac, &QAction::triggered, this, [this](){
        auto sv = new CSnippetViewer(snv->parentWnd());
        sv->txtBrowser->page()->setInspectedPage(snv->txtBrowser->page());
    });
#endif

    ccm->addSeparator();
    ccm->addAction(QIcon::fromTheme(QStringLiteral("documentation")),tr("Show in editor"),
                 this,&CSnCtxHandler::showInEditor);

    ac = new QAction(QIcon::fromTheme(QStringLiteral("download")),tr("Open in browser"),nullptr);
    ac->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
    connect(ac, &QAction::triggered,this,[this](){
        if (!QProcess::startDetached(gSet->settings()->sysBrowser,
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start browser."));
    });
    ccm->addAction(ac);

    ac = new QAction(QIcon::fromTheme(QStringLiteral("system-run")),
                     tr("Open with associated application"),nullptr);
    connect(ac, &QAction::triggered,this,[this](){
        if (!QProcess::startDetached(QStringLiteral("xdg-open"),
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start associated application."));
    });
    ccm->addAction(ac);

    ccm->addSeparator();

    ccm->addAction(QIcon::fromTheme(QStringLiteral("download-later")),tr("Download all images"),
                   snv->transHandler,&CSnTrans::getImgUrlsAndParse);

    if (origin.toString().startsWith(
                QStringLiteral("https://www.pixiv.net/member_illust.php?mode=medium"),
                Qt::CaseInsensitive)) {

        ac = ccm->addAction(tr("Download all images from Pixiv illustration"),
                      snv->netHandler,&CSnNet::downloadPixivManga);
        ac->setData(origin);

        QUrl fiurl(QStringLiteral("https://www.pixiv.net/favicon.ico"));
        auto fl = new CFaviconLoader(snv,fiurl);
        connect(fl,&CFaviconLoader::gotIcon,ac,[ac](const QIcon& icon){
            ac->setIcon(icon);
        });
        fl->queryStart(false);
    }
    ccm->addSeparator();

    ccm->addAction(QIcon::fromTheme(QStringLiteral("document-edit-decrypt")),tr("Parse document"),
                   snv->transHandler,&CSnTrans::reparseDocument);
    ccm->addSeparator();
    ccm->addAction(snv->parentWnd()->actionFullscreen);
    ccm->addAction(QIcon::fromTheme(QStringLiteral("dialog-close")),tr("Close tab"),
                 snv,&CSnippetViewer::closeTab);
    ccm->addSeparator();
    if (!sText.isEmpty()) {
        ac = ccm->addAction(QIcon::fromTheme(QStringLiteral("document-save-as")),
                            tr("Save selected text as file..."),
                            this,&CSnCtxHandler::saveToFile);
        ac->setData(sText);
    }
    ccm->addAction(QIcon::fromTheme(QStringLiteral("document-save")),tr("Save to file..."),
                 this,&CSnCtxHandler::saveToFile);

    m_menuActive->start();

    connect(cm, &QMenu::aboutToHide, this, [this](){
        m_menuActive->stop();
    });

    cm->setAttribute(Qt::WA_DeleteOnClose,true);
    cm->popup(snv->txtBrowser->mapToGlobal(pos));
}

void CSnCtxHandler::reconfigureDefaultActions()
{
    QWebEnginePage* p = snv->txtBrowser->page();
    if (p==nullptr) return;

    p->action(QWebEnginePage::Paste)->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
    p->action(QWebEnginePage::SelectAll)->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-all")));
    p->action(QWebEnginePage::OpenLinkInNewTab)->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    p->action(QWebEnginePage::OpenLinkInNewTab)->setText(tr("Open in new tab"));
    p->action(QWebEnginePage::OpenLinkInNewBackgroundTab)->
            setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    p->action(QWebEnginePage::OpenLinkInNewBackgroundTab)->setText(tr("Open in new background tab"));
    p->action(QWebEnginePage::CopyLinkToClipboard)->setText(tr("Copy link url to clipboard"));
    p->action(QWebEnginePage::CopyImageToClipboard)->setText(tr("Copy image to clipboard"));
    p->action(QWebEnginePage::CopyImageUrlToClipboard)->setText(tr("Copy image url to clipboard"));
    p->action(QWebEnginePage::DownloadImageToDisk)->setText(tr("Save image to file..."));
}

bool CSnCtxHandler::isMenuTimerActive()
{
    if (m_menuActive!=nullptr)
        return m_menuActive->isActive();

    return false;
}

void CSnCtxHandler::translateFragment()
{
    auto nt = qobject_cast<QAction *>(sender());
    if (nt==nullptr) return;
    QString s = nt->data().toString();
    if (s.isEmpty()) return;
    CLangPair lp(gSet->ui()->getActiveLangPair());
    if (!lp.isValid()) return;

    auto th = new QThread();
    auto at = new CAuxTranslator();
    at->setText(s);
    at->setSrcLang(lp.langFrom.bcp47Name());
    at->setDestLang(lp.langTo.bcp47Name());
    connect(this,&CSnCtxHandler::startTranslation,
            at,&CAuxTranslator::startTranslation,Qt::QueuedConnection);

    connect(at,&CAuxTranslator::gotTranslation,this,[this](const QString& text){
        if (!text.isEmpty() && !m_menuActive->isActive()) {
            CSpecToolTipLabel* lbl = new CSpecToolTipLabel(wordWrap(text,maxTranslateFragmentCharWidth));
            lbl->setStyleSheet(QStringLiteral("QLabel { background: #fefdeb; color: black; }"));
            connect(this, &CSnCtxHandler::hideTooltips,lbl, &CSpecToolTipLabel::close);
            QxtToolTip::show(QCursor::pos(),lbl,snv->parentWnd());
        }
    },Qt::QueuedConnection);

    at->moveToThread(th);
    th->start();

    Q_EMIT startTranslation(true);
}

void CSnCtxHandler::saveToFile()
{
    QString selectedText;
    selectedText.clear();
    auto nt = qobject_cast<QAction *>(sender());
    if (nt)
        selectedText = nt->data().toString();

    QString fname;
    if (selectedText.isEmpty()) {
        fname = getSaveFileNameD(snv,tr("Save to HTML"),gSet->settings()->savedAuxSaveDir,
                                 tr("HTML file (*.htm);;Complete HTML with files (*.html);;"
                                    "Text file (*.txt);;MHTML archive (*.mht)"));
    } else {
        fname = getSaveFileNameD(snv,tr("Save to file"),gSet->settings()->savedAuxSaveDir,
                                 tr("Text file (*.txt)"));
    }

    if (fname.isNull() || fname.isEmpty()) return;
    gSet->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

    QFileInfo fi(fname);
    if (!selectedText.isEmpty()) {
        QFile f(fname);
        f.open(QIODevice::WriteOnly|QIODevice::Truncate);
        f.write(selectedText.toUtf8());
        f.close();

    } else if (fi.suffix().toLower().startsWith(QStringLiteral("txt"))) {
        snv->txtBrowser->page()->toPlainText([fname](const QString& result)
        {
            QFile f(fname);
            f.open(QIODevice::WriteOnly|QIODevice::Truncate);
            f.write(result.toUtf8());
            f.close();
        });

    } else if (fi.suffix().toLower().startsWith(QStringLiteral("mht"))) {
        snv->txtBrowser->page()->save(fname,QWebEngineDownloadItem::MimeHtmlSaveFormat);

    } else if (fi.suffix().toLower()==QStringLiteral("htm")) {
        snv->txtBrowser->page()->save(fname,QWebEngineDownloadItem::SingleHtmlSaveFormat);

    } else {
        snv->txtBrowser->page()->save(fname,QWebEngineDownloadItem::CompleteHtmlSaveFormat);
    }
}

void CSnCtxHandler::bookmarkPage() {
    QWebEnginePage *lf = snv->txtBrowser->page();
    auto dlg = new AddBookmarkDialog(lf->requestedUrl().toString(),lf->title(),snv);
    if (dlg->exec() == QDialog::Accepted)
        Q_EMIT gSet->updateAllBookmarks();
    dlg->setParent(nullptr);
    delete dlg;
}

void CSnCtxHandler::showInEditor()
{
    QString fname = gSet->makeTmpFileName(QStringLiteral("html"),true);
    snv->txtBrowser->page()->toHtml([fname,this](const QString& html) {
        QFile tfile(fname);
        tfile.open(QIODevice::WriteOnly);
        tfile.write(html.toUtf8());
        tfile.close();
        gSet->appendCreatedFiles(fname);

        if (!QProcess::startDetached(gSet->settings()->sysEditor, QStringList() << fname))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start editor."));
    });
}

void CSnCtxHandler::showSource()
{
    auto srcv = new CSourceViewer(snv);
    srcv->showNormal();
}

void CSnCtxHandler::runJavaScript()
{
    auto nt = qobject_cast<QAction *>(sender());
    if (nt==nullptr) return;
    if (!nt->data().canConvert<QString>()) return;

    const QString source = nt->data().toString();

    if (!source.isEmpty())
        snv->txtBrowser->page()->runJavaScript(source,QWebEngineScript::MainWorld);
}
