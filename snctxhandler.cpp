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

namespace CDefaults {
const int menuActiveTimerInterval = 1000;
const int maxRealmLabelEliding = 60;
const int maxTranslateFragmentCharWidth = 80;
}

CSnCtxHandler::CSnCtxHandler(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    m_menuActive.setSingleShot(false);
    m_menuActive.setInterval(CDefaults::menuActiveTimerInterval);

    connect(&m_menuActive, &QTimer::timeout, this, [this](){
        Q_EMIT hideTooltips();
    });

    connect(snv->txtBrowser,&CSpecWebView::contextMenuRequested,this,&CSnCtxHandler::contextMenu);
}

void CSnCtxHandler::contextMenu(const QPoint &pos, const QWebEngineContextMenuData& data)
{
    QString sText;
    QUrl linkUrl;
    QUrl imageUrl;
    QUrl pixivUrl;
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
    QVector<QAction *> pixivActions;

    QMenu m_menu;

    if (linkUrl.isValid()) {
        QUrl nUrl = linkUrl;
        if (CGenericFuncs::checkAndUnpackUrl(nUrl)) {
            ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                             tr("Open in new background tab"));
            connect(ac, &QAction::triggered, this, [nUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),nUrl,QStringList(),false);
            });
            ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                             tr("Open in new tab"));
            connect(ac, &QAction::triggered, this, [nUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),nUrl,QStringList());
            });
        } else {
            m_menu.addAction(snv->txtBrowser->pageAction(QWebEnginePage::OpenLinkInNewBackgroundTab));
            m_menu.addAction(snv->txtBrowser->pageAction(QWebEnginePage::OpenLinkInNewTab));
        }

        ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                         tr("Open in new background tab and translate"));
        connect(ac, &QAction::triggered, this, [linkUrl,this]() {
            auto sn = new CSnippetViewer(snv->parentWnd(),linkUrl,QStringList(),false);
            sn->m_requestAutotranslate = true;
        });

        m_menu.addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyLinkToClipboard));
        m_menu.addSeparator();
    }

    QString title = snv->txtBrowser->page()->title();
    pixivUrl.clear();
    if (!snv->m_fileChanged)
        pixivUrl = QUrl::fromUserInput(snv->urlEdit->text());
    if (linkUrl.isValid()) {
        pixivUrl = linkUrl;
        title.clear();
    }
    if (!pixivUrl.host().contains(QStringLiteral("pixiv.net")))
        pixivUrl.clear();
    pixivUrl.setFragment(QString());
    QUrlQuery puq(pixivUrl);
    QString pixivId = puq.queryItemValue(QStringLiteral("id"));
    pixivId.remove(QRegExp(QStringLiteral("[^0-9]")));

    if (pixivUrl.isValid() && pixivUrl.toString().contains(
             QStringLiteral("pixiv.net/novel/show.php"), Qt::CaseInsensitive)) {
        ac = m_menu.addAction(tr("Extract pixiv novel in new background tab"));
        connect(ac, &QAction::triggered, this, [this,pixivUrl,title](){
            snv->netHandler->processPixivNovel(pixivUrl,title,false,false);
        });
        pixivActions.append(ac);

        ac = m_menu.addAction(tr("Extract pixiv novel in new tab"));
        connect(ac, &QAction::triggered, this, [this,pixivUrl,title](){
            snv->netHandler->processPixivNovel(pixivUrl,title,false,true);
        });
        pixivActions.append(ac);

        ac = m_menu.addAction(tr("Extract pixiv novel in new background tab and translate"));
        connect(ac, &QAction::triggered, this, [this,pixivUrl,title](){
            snv->netHandler->processPixivNovel(pixivUrl,title,true,false);
        });
        pixivActions.append(ac);

        m_menu.addSeparator();
    }

    if ((pixivUrl.isValid() && !pixivId.isEmpty() && (
             pixivUrl.toString().contains(QStringLiteral("pixiv.net/member.php"), Qt::CaseInsensitive) ||
             pixivUrl.toString().contains(QStringLiteral("pixiv.net/novel/member.php"), Qt::CaseInsensitive) ||
             pixivUrl.toString().contains(QStringLiteral("pixiv.net/novel/bookmark.php"), Qt::CaseInsensitive)))) {

        ac = m_menu.addAction(tr("Extract novel list for author in new tab"));
        connect(ac, &QAction::triggered, this, [this,pixivId](){
            snv->netHandler->processPixivNovelList(pixivId,CPixivIndexExtractor::WorkIndex);
        });
        pixivActions.append(ac);

        ac = m_menu.addAction(tr("Extract novel bookmarks list for author in new tab"));
        connect(ac, &QAction::triggered, this, [this,pixivId](){
            snv->netHandler->processPixivNovelList(pixivId,CPixivIndexExtractor::BookmarksIndex);
        });
        pixivActions.append(ac);

        m_menu.addSeparator();
    }

    if (!sText.isEmpty()) {
        m_menu.addAction(snv->txtBrowser->page()->action(QWebEnginePage::Copy));

        ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("document-edit-verify")),
                         tr("Translate"));
        ac->setData(sText);
        connect(ac, &QAction::triggered,this, &CSnCtxHandler::translateFragment);

        ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new-background")),
                         tr("Create plain text in separate tab"));
        connect(ac, &QAction::triggered, this, [sText,this](){
            QString s = sText;
            s = s.replace('\n',QStringLiteral("<br/>"));
            s = CGenericFuncs::makeSimpleHtml(tr("Text, %1 length").arg(s.length()),s);
            new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),true,s);
        });

        ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new-background")),
                         tr("Translate plain text in separate tab"));
        connect(ac, &QAction::triggered, this, [sText,this](){
            QString s = sText;
            s = s.replace('\n',QStringLiteral("<br/>"));
            s = CGenericFuncs::makeSimpleHtml(tr("Text, %1 length").arg(s.length()),s);
            CSnippetViewer* sn = new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),true,s);
            sn->m_requestAutotranslate = true;
        });

        QStringList searchNames = gSet->getSearchEngines();
        if (!searchNames.isEmpty()) {
            m_menu.addSeparator();

            searchNames.sort(Qt::CaseInsensitive);
            for (const QString& name : qAsConst(searchNames)) {
                QUrl url = gSet->createSearchUrl(sText,name);

                ac = m_menu.addAction(name);
                connect(ac, &QAction::triggered, this, [url,this](){
                    new CSnippetViewer(snv->parentWnd(), url);
                });

                QUrl fiurl = url;
                fiurl.setFragment(QString());
                fiurl.setQuery(QString());
                fiurl.setPath(QStringLiteral("/favicon.ico"));
                auto fl = new CFaviconLoader(snv,fiurl);
                connect(fl,&CFaviconLoader::finished,fl,&CFaviconLoader::deleteLater);
                connect(fl,&CFaviconLoader::gotIcon,ac,[ac](const QIcon& icon){
                    ac->setIcon(icon);
                });
                fl->queryStart(false);
            }
        }

        m_menu.addSeparator();

        ac = m_menu.addAction(QIcon(QStringLiteral(":/img/nepomuk")),
                         tr("Local indexed search"));
        connect(ac, &QAction::triggered, this, [sText,this](){
            auto bt = new CSearchTab(snv->parentWnd());
            bt->searchTerm(sText);
        });

        ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("edit-find")),
                         tr("Search on page"));
        connect(ac, &QAction::triggered, this, [sText,this](){
            snv->searchPanel->show();
            snv->searchEdit->setEditText(sText);
            snv->fwdButton->click();
        });

        ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("accessories-dictionary")),
                         tr("Local dictionary"));
        connect(ac, &QAction::triggered, this, [sText](){
            gSet->showDictionaryWindowEx(sText);
        });

        ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("document-edit-verify")),
                         tr("Light translator"));
        connect(ac, &QAction::triggered, this, [sText](){
            gSet->showLightTranslator(sText);
        });

        QUrl selectedUrl = QUrl::fromUserInput(sText);
        if (selectedUrl.isValid() && !selectedUrl.isLocalFile() && !selectedUrl.isRelative()) {
            m_menu.addSeparator();

            QMenu* icm = m_menu.addMenu(QIcon::fromTheme(QStringLiteral("go-jump-locationbar")),
                                     tr("Open selected link"));

            ac = icm->addAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                             tr("Open in new background tab"));
            connect(ac, &QAction::triggered, this, [selectedUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),selectedUrl,QStringList(),false);
            });

            ac = icm->addAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                             tr("Open in new tab"));
            connect(ac, &QAction::triggered, this, [selectedUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),selectedUrl,QStringList());
            });

            ac = icm->addAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                             tr("Open in new background tab and translate"));
            connect(ac, &QAction::triggered, this, [selectedUrl,this]() {
                CSnippetViewer* sn = new CSnippetViewer(snv->parentWnd(),selectedUrl,QStringList(),false);
                sn->m_requestAutotranslate = true;
            });
        }

        m_menu.addSeparator();
    }

    if (imageUrl.isValid()) {
        QMenu* icm = m_menu.addMenu(QIcon::fromTheme(QStringLiteral("view-preview")),
                                 tr("Image"));

        icm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyImageToClipboard));
        icm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyImageUrlToClipboard));
        icm->addSeparator();

        ac = icm->addAction(tr("Open image in new tab"));
        connect(ac,&QAction::triggered,this,[this,imageUrl](){
            new CSnippetViewer(snv->parentWnd(),imageUrl);
        });

        icm->addAction(snv->txtBrowser->pageAction(QWebEnginePage::DownloadImageToDisk));
        m_menu.addSeparator();
    }

    ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("tab-duplicate")),
                     tr("Duplicate tab"));
    connect(ac, &QAction::triggered, this, [this](){
        QString url(QStringLiteral("about://blank"));
        if (!snv->m_fileChanged) url=snv->urlEdit->text();

        CSnippetViewer* sv = new CSnippetViewer(snv->parentWnd(), url);

        if (snv->m_fileChanged || url.isEmpty()) {
            snv->txtBrowser->page()->toHtml([sv,this](const QString& html) {
                if (!html.isEmpty()) {
                    if (html.toUtf8().size()<CDefaults::maxDataUrlFileSize) {
                        sv->txtBrowser->setHtmlInterlocked(html,snv->txtBrowser->page()->url());
                    } else {
                        sv->netHandler->loadWithTempFile(html,false);
                    }
                }
            });
        }
    });

    m_menu.addAction(QIcon::fromTheme(QStringLiteral("tab-detach")),tr("Detach tab"),
                  snv,&CSnippetViewer::detachTab);

    m_menu.addSeparator();
    m_menu.addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")),
                  tr("Add current page to bookmarks"),
                  this, &CSnCtxHandler::bookmarkPage);

    m_menu.addSeparator();
    m_menu.addAction(QIcon::fromTheme(QStringLiteral("go-previous")),tr("Back"),
                  snv->txtBrowser,&CSpecWebView::back,QKeySequence(Qt::CTRL + Qt::Key_Z));
    m_menu.addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),tr("Reload"),
                  snv->txtBrowser,&CSpecWebView::reload,QKeySequence(Qt::CTRL + Qt::Key_R));

    if (cb->mimeData(QClipboard::Clipboard)->hasText())
        m_menu.addAction(snv->txtBrowser->page()->action(QWebEnginePage::Paste));
    m_menu.addAction(snv->txtBrowser->page()->action(QWebEnginePage::SelectAll));
    m_menu.addSeparator();

    const QVector<CUserScript> scripts = gSet->getUserScriptsForUrl(origin,false,true);
    if (!scripts.isEmpty()) {
        ccm = m_menu.addMenu(QIcon::fromTheme(QStringLiteral("run-build")),
                          tr("Start user script"));
        for (const auto& script : scripts) {
            ac = ccm->addAction(script.getTitle());
            ac->setData(script.getSource());
        }
        connect(ac,&QAction::triggered,this,&CSnCtxHandler::runJavaScript);
    }
    if (gSet->settings()->useNoScript) {
        ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("dialog-cancel")),
                         tr("Enable scripts for this page..."));
        connect(ac, &QAction::triggered, this, [this](){
            const QUrl origin = snv->getUrl();
            if (origin.isLocalFile() || origin.isRelative()) return;
            QString u = snv->getUrl().toString(CSettings::adblockUrlFmt);
            auto dlg = new CNoScriptDialog(snv,u);
            dlg->exec();
            dlg->setParent(nullptr);
            delete dlg;
        });
    }
    if (!scripts.isEmpty() || gSet->settings()->useNoScript)
        m_menu.addSeparator();

    if (!linkUrl.isEmpty()) {
        ac = m_menu.addAction(QIcon::fromTheme(QStringLiteral("preferences-web-browser-adblock")),
                         tr("Add AdBlock rule for link url..."));
        connect(ac, &QAction::triggered, this, [linkUrl,this](){
            QString u = linkUrl.toString();
            bool ok;
            u = QInputDialog::getText(snv, tr("Add AdBlock rule"), tr("Filter template"),
                                      QLineEdit::Normal, u, &ok);
            if (ok)
                gSet->adblockAppend(u);
        });
        m_menu.addSeparator();
    }

    ccm = m_menu.addMenu(QIcon::fromTheme(QStringLiteral("dialog-password")),
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
            QString realm = CGenericFuncs::elideString(gSet->cleanUrlForRealm(origin).toString(),
                                                       CDefaults::maxRealmLabelEliding,Qt::ElideLeft);
            auto dlg = new CAuthDlg(QApplication::activeWindow(),origin,realm,true);
            dlg->exec();
            dlg->setParent(nullptr);
            delete dlg;
        });
    }
    m_menu.addSeparator();

    ccm = m_menu.addMenu(QIcon::fromTheme(QStringLiteral("system-run")),tr("Service"));
    ccm->addAction(QIcon::fromTheme(QStringLiteral("document-edit-verify")),
                   tr("Show source"),
                   this,&CSnCtxHandler::showSource,QKeySequence(Qt::CTRL + Qt::Key_E));

    ac = ccm->addAction(tr("Inspect page"));
    connect(ac, &QAction::triggered, snv->msgHandler, &CSnMsgHandler::showInspector);

    ccm->addSeparator();
    ccm->addAction(QIcon::fromTheme(QStringLiteral("documentation")),tr("Show in editor"),
                   this,&CSnCtxHandler::showInEditor);

    ac = ccm->addAction(QIcon::fromTheme(QStringLiteral("download")),tr("Open in browser"));
    ac->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
    connect(ac, &QAction::triggered,this,[this](){
        if (!QProcess::startDetached(gSet->settings()->sysBrowser,
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start browser."));
    });

    ac = ccm->addAction(QIcon::fromTheme(QStringLiteral("system-run")),
                     tr("Open with associated application"));
    connect(ac, &QAction::triggered,this,[this](){
        if (!QProcess::startDetached(QStringLiteral("xdg-open"),
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start associated application."));
    });

    ccm->addSeparator();

    ccm->addAction(QIcon::fromTheme(QStringLiteral("download-later")),tr("Download all images"),
                   snv->transHandler,&CSnTrans::getImgUrlsAndParse);

    if (origin.toString().startsWith(
                QStringLiteral("https://www.pixiv.net/member_illust.php?mode=medium"),
                Qt::CaseInsensitive)) {

        ac = ccm->addAction(tr("Download all images from Pixiv illustration"),
                            snv->netHandler,&CSnNet::downloadPixivManga);
        ac->setData(origin);
        pixivActions.append(ac);
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

    if (!pixivActions.isEmpty()) {
        QUrl pixivIconUrl(QStringLiteral("http://www.pixiv.net/favicon.ico"));
        if (pixivUrl.isValid())
            pixivIconUrl.setScheme(pixivUrl.scheme());
        auto fl = new CFaviconLoader(snv,pixivIconUrl);
        connect(fl,&CFaviconLoader::finished,fl,&CFaviconLoader::deleteLater);
        connect(fl,&CFaviconLoader::gotIcon, &m_menu, [pixivActions](const QIcon& icon){
            for (auto &ac : qAsConst(pixivActions)) {
                ac->setIcon(icon);
            }
        });
        fl->queryStart(false);
    }

    m_menuActive.start();

    connect(&m_menu, &QMenu::aboutToHide, this, [this](){
        m_menuActive.stop();
    });

    m_menu.exec(snv->txtBrowser->mapToGlobal(pos));
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

bool CSnCtxHandler::isMenuActive()
{
    return m_menuActive.isActive();
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
    connect(th,&QThread::finished,at,&CAuxTranslator::deleteLater);
    connect(th,&QThread::finished,th,&QThread::deleteLater);

    connect(at,&CAuxTranslator::gotTranslation,this,[this](const QString& text){
        if (!text.isEmpty() && !isMenuActive()) {
            auto lbl = new QLabel(CGenericFuncs::wordWrap(
                                      text,CDefaults::maxTranslateFragmentCharWidth));
            lbl->setStyleSheet(QStringLiteral("QLabel { background: #fefdeb; color: black; }"));
            connect(this, &CSnCtxHandler::hideTooltips,lbl, &QLabel::close);
            QxtToolTip::show(QCursor::pos(),lbl,snv->parentWnd(),QRect(),false,true);
        }
    },Qt::QueuedConnection);

    at->moveToThread(th);
    th->start();

    QMetaObject::invokeMethod(at,&CAuxTranslator::translateAndQuit,Qt::QueuedConnection);
}

void CSnCtxHandler::saveToFile()
{
    const int fFullHtml = 0;
    const int fHtml = 1;
    const int fMHT = 2;
    const int fTxt = 3;
    static const QStringList filters ( { tr("Complete HTML (*.html)"),
                                         tr("HTML file (*.html)"),
                                         tr("MHTML archive (*.mhtml)"),
                                         tr("Text file (*.txt)") } );

    QString selectedText;
    auto nt = qobject_cast<QAction *>(sender());
    if (nt)
        selectedText = nt->data().toString();

    QString fname;
    QString selectedFilter;
    if (selectedText.isEmpty()) {
        fname = CGenericFuncs::getSaveFileNameD(snv,tr("Save to HTML"),gSet->settings()->savedAuxSaveDir,
                                                filters,&selectedFilter);
    } else {
        fname = CGenericFuncs::getSaveFileNameD(snv,tr("Save to file"),gSet->settings()->savedAuxSaveDir,
                                                QStringList( { filters.at(fTxt) } ));
    }

    if (fname.isNull() || fname.isEmpty()) return;
    gSet->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

    if (!selectedText.isEmpty()) {
        QFile f(fname);
        f.open(QIODevice::WriteOnly|QIODevice::Truncate);
        f.write(selectedText.toUtf8());
        f.close();

    } else {
        int fmt = filters.indexOf(selectedFilter);
        if (fmt == fTxt) {
            snv->txtBrowser->page()->toPlainText([fname](const QString& result)
            {
                QFile f(fname);
                f.open(QIODevice::WriteOnly|QIODevice::Truncate);
                f.write(result.toUtf8());
                f.close();
            });

        } else if (fmt == fMHT) {
            snv->txtBrowser->page()->save(fname,QWebEngineDownloadItem::MimeHtmlSaveFormat);

        } else if (fmt == fHtml) {
            snv->txtBrowser->page()->save(fname,QWebEngineDownloadItem::SingleHtmlSaveFormat);

        } else if (fmt == fFullHtml){
            snv->txtBrowser->page()->save(fname,QWebEngineDownloadItem::CompleteHtmlSaveFormat);

        } else {
            qCritical() << "Unknown selected filter" << selectedFilter;
        }
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
