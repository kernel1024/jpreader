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
#include <QRegularExpression>

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
#include "extractors/htmlimagesextractor.h"

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
    const QUrl origin = snv->txtBrowser->page()->url();

    if (data.isValid()) {
        sText = data.selectedText();
        linkUrl = data.linkUrl();
        if (data.mediaType()==QWebEngineContextMenuData::MediaTypeImage)
            imageUrl = data.mediaUrl();
    }

    const QClipboard *cb = QApplication::clipboard();
    QAction *ac = nullptr;
    QMenu *ccm = nullptr;

    QMenu m_menu;

    if (linkUrl.isValid()) {
        QUrl nUrl = linkUrl;
        if (CGenericFuncs::checkAndUnpackUrl(nUrl)) {
            ac = m_menu.addAction(QIcon::fromTheme(QSL("tab-new")),
                             tr("Open in new background tab"));
            connect(ac, &QAction::triggered, this, [nUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),nUrl,QStringList(),false);
            });
            ac = m_menu.addAction(QIcon::fromTheme(QSL("tab-new")),
                             tr("Open in new tab"));
            connect(ac, &QAction::triggered, this, [nUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),nUrl,QStringList());
            });
        } else {
            m_menu.addAction(snv->txtBrowser->pageAction(QWebEnginePage::OpenLinkInNewBackgroundTab));
            m_menu.addAction(snv->txtBrowser->pageAction(QWebEnginePage::OpenLinkInNewTab));
        }

        ac = m_menu.addAction(QIcon::fromTheme(QSL("tab-new")),
                         tr("Open in new background tab and translate"));
        connect(ac, &QAction::triggered, this, [linkUrl,this]() {
            auto *sn = new CSnippetViewer(snv->parentWnd(),linkUrl,QStringList(),false);
            sn->m_requestAutotranslate = true;
        });

        m_menu.addAction(snv->txtBrowser->pageAction(QWebEnginePage::CopyLinkToClipboard));
        m_menu.addSeparator();
    }

    QString title = snv->txtBrowser->page()->title();
    QUrl pageUrl;
    if (!snv->m_fileChanged)
        pageUrl = QUrl::fromUserInput(snv->urlEdit->text());
    if (linkUrl.isValid()) {
        pageUrl = linkUrl;
        title.clear();
    }

    auto extractorsList = CAbstractExtractor::addMenuActions(pageUrl,origin,title,&m_menu,snv);
    for (auto *eac : extractorsList) {
        const auto params = eac->data().toHash();
        if (params.contains(QSL("html"))) {
            connect(eac,&QAction::triggered,this,[this,params](){
                snv->txtBrowser->page()->toHtml([this,params](const QString& html){
                    QHash<QString,QVariant> pl = params;
                    pl[QSL("html")] = html;
                    snv->netHandler->processExtractorActionIndirect(pl);
                });
            });
        } else {
            connect(eac,&QAction::triggered,snv->netHandler,&CSnNet::processExtractorAction);
        }
    }
    m_menu.addActions(extractorsList);

    if (!sText.isEmpty()) {
        m_menu.addAction(snv->txtBrowser->page()->action(QWebEnginePage::Copy));

        ac = m_menu.addAction(QIcon::fromTheme(QSL("document-edit-verify")),
                         tr("Translate"));
        ac->setData(sText);
        connect(ac, &QAction::triggered,this, &CSnCtxHandler::translateFragment);

        ac = m_menu.addAction(QIcon::fromTheme(QSL("tab-new-background")),
                         tr("Create plain text in separate tab"));
        connect(ac, &QAction::triggered, this, [sText,this](){
            QString s = sText;
            s = s.replace('\n',QSL("<br/>"));
            s = CGenericFuncs::makeSimpleHtml(tr("Text, %1 length").arg(s.length()),s);
            new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),true,s);
        });

        ac = m_menu.addAction(QIcon::fromTheme(QSL("tab-new-background")),
                         tr("Translate plain text in separate tab"));
        connect(ac, &QAction::triggered, this, [sText,this](){
            QString s = sText;
            s = s.replace('\n',QSL("<br/>"));
            s = CGenericFuncs::makeSimpleHtml(tr("Text, %1 length").arg(s.length()),s);
            auto *sn = new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),true,s);
            sn->m_requestAutotranslate = true;
        });

        m_menu.addSeparator();

        ac = m_menu.addAction(QIcon::fromTheme(QSL("document-edit")),
                         tr("Extract HTML via clipboard to separate tab"));
        connect(ac, &QAction::triggered, snv->netHandler, &CSnNet::extractHTMLFragment);

        QStringList searchNames = gSet->getSearchEngines();
        if (!searchNames.isEmpty()) {
            m_menu.addSeparator();
            ccm = m_menu.addMenu(QIcon::fromTheme(QSL("edit-web-search")),tr("Search with"));

            searchNames.sort(Qt::CaseInsensitive);
            for (const QString& name : qAsConst(searchNames)) {
                QUrl url = gSet->createSearchUrl(sText,name);

                ac = ccm->addAction(name);
                connect(ac, &QAction::triggered, this, [url,this](){
                    new CSnippetViewer(snv->parentWnd(), url);
                });

                QUrl fiurl = url;
                fiurl.setFragment(QString());
                fiurl.setQuery(QString());
                fiurl.setPath(QSL("/favicon.ico"));
                auto *fl = new CFaviconLoader(snv,fiurl);
                connect(fl,&CFaviconLoader::finished,fl,&CFaviconLoader::deleteLater);
                connect(fl,&CFaviconLoader::gotIcon,ac,[ac](const QIcon& icon){
                    ac->setIcon(icon);
                });
                fl->queryStart(false);
            }
        }

        m_menu.addSeparator();

        ac = m_menu.addAction(QIcon(QSL(":/img/nepomuk")),
                         tr("Local indexed search"));
        connect(ac, &QAction::triggered, this, [sText,this](){
            auto *bt = new CSearchTab(snv->parentWnd());
            bt->searchTerm(sText);
        });

        ac = m_menu.addAction(QIcon::fromTheme(QSL("edit-find")),
                         tr("Search on page"));
        connect(ac, &QAction::triggered, this, [sText,this](){
            snv->searchPanel->show();
            snv->searchEdit->setEditText(sText);
            snv->fwdButton->click();
        });

        ac = m_menu.addAction(QIcon::fromTheme(QSL("accessories-dictionary")),
                         tr("Local dictionary"));
        connect(ac, &QAction::triggered, this, [sText](){
            gSet->showDictionaryWindowEx(sText);
        });

        ac = m_menu.addAction(QIcon::fromTheme(QSL("document-edit-verify")),
                         tr("Light translator"));
        connect(ac, &QAction::triggered, this, [sText](){
            gSet->showLightTranslator(sText);
        });

        QUrl selectedUrl = QUrl::fromUserInput(sText);
        if (selectedUrl.isValid() && !selectedUrl.isLocalFile() && !selectedUrl.isRelative()) {
            m_menu.addSeparator();

            QMenu* icm = m_menu.addMenu(QIcon::fromTheme(QSL("go-jump-locationbar")),
                                     tr("Open selected link"));

            ac = icm->addAction(QIcon::fromTheme(QSL("tab-new")),
                             tr("Open in new background tab"));
            connect(ac, &QAction::triggered, this, [selectedUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),selectedUrl,QStringList(),false);
            });

            ac = icm->addAction(QIcon::fromTheme(QSL("tab-new")),
                             tr("Open in new tab"));
            connect(ac, &QAction::triggered, this, [selectedUrl,this]() {
                new CSnippetViewer(snv->parentWnd(),selectedUrl,QStringList());
            });

            ac = icm->addAction(QIcon::fromTheme(QSL("tab-new")),
                             tr("Open in new background tab and translate"));
            connect(ac, &QAction::triggered, this, [selectedUrl,this]() {
                auto *sn = new CSnippetViewer(snv->parentWnd(),selectedUrl,QStringList(),false);
                sn->m_requestAutotranslate = true;
            });
        }

        m_menu.addSeparator();
    }

    if (imageUrl.isValid()) {
        QMenu* icm = m_menu.addMenu(QIcon::fromTheme(QSL("view-preview")),
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

    ac = m_menu.addAction(QIcon::fromTheme(QSL("tab-duplicate")),
                     tr("Duplicate tab"));
    connect(ac, &QAction::triggered, this, [this](){
        QString url(QSL("about://blank"));
        if (!snv->m_fileChanged) url=snv->urlEdit->text();

        auto *sv = new CSnippetViewer(snv->parentWnd(), url);

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

    m_menu.addAction(QIcon::fromTheme(QSL("tab-detach")),tr("Detach tab"),
                  snv,&CSnippetViewer::detachTab);

    m_menu.addSeparator();
    m_menu.addAction(QIcon::fromTheme(QSL("bookmark-new")),
                  tr("Add current page to bookmarks"),
                  this, &CSnCtxHandler::bookmarkPage);

    m_menu.addSeparator();
    m_menu.addAction(QIcon::fromTheme(QSL("go-previous")),tr("Back"),
                  snv->txtBrowser,&CSpecWebView::back,QKeySequence(Qt::CTRL + Qt::Key_Z));
    m_menu.addAction(QIcon::fromTheme(QSL("view-refresh")),tr("Reload"),
                  snv->txtBrowser,&CSpecWebView::reload,QKeySequence(Qt::CTRL + Qt::Key_R));

    if (cb->mimeData(QClipboard::Clipboard)->hasText())
        m_menu.addAction(snv->txtBrowser->page()->action(QWebEnginePage::Paste));
    m_menu.addAction(snv->txtBrowser->page()->action(QWebEnginePage::SelectAll));
    m_menu.addSeparator();

    const QVector<CUserScript> scripts = gSet->getUserScriptsForUrl(origin,false,true,false);
    if (!scripts.isEmpty()) {
        ccm = m_menu.addMenu(QIcon::fromTheme(QSL("run-build")),
                          tr("Start user script"));
        for (const auto& script : scripts) {
            ac = ccm->addAction(script.getTitle());
            ac->setData(script.getSource());
        }
        connect(ac,&QAction::triggered,this,&CSnCtxHandler::runJavaScript);
    }
    if (gSet->settings()->useNoScript) {
        ac = m_menu.addAction(QIcon::fromTheme(QSL("dialog-cancel")),
                         tr("Enable scripts for this page..."));
        connect(ac, &QAction::triggered, this, [this](){
            const QUrl origin = snv->getUrl();
            if (origin.isLocalFile() || origin.isRelative()) return;
            QString u = snv->getUrl().toString(CSettings::adblockUrlFmt);
            CNoScriptDialog dlg(snv,u);
            dlg.exec();
        });
    }
    if (!scripts.isEmpty() || gSet->settings()->useNoScript)
        m_menu.addSeparator();

    if (!linkUrl.isEmpty()) {
        ac = m_menu.addAction(QIcon::fromTheme(QSL("preferences-web-browser-adblock")),
                         tr("Add AdBlock rule for link url..."));
        connect(ac, &QAction::triggered, this, [linkUrl,this](){
            QString u = linkUrl.toString();
            bool ok = false;
            u = QInputDialog::getText(snv, tr("Add AdBlock rule"), tr("Filter template"),
                                      QLineEdit::Normal, u, &ok);
            if (ok)
                gSet->adblockAppend(u);
        });
        m_menu.addSeparator();
    }

    ccm = m_menu.addMenu(QIcon::fromTheme(QSL("dialog-password")),
                      tr("Form autofill"));
    if (gSet->haveSavedPassword(origin)) {
        ac = ccm->addAction(QIcon::fromTheme(QSL("tools-wizard")),
                            tr("Insert username and password"),
                            snv->msgHandler,&CSnMsgHandler::pastePassword);
        ac->setData(1);
        ac = ccm->addAction(tr("Insert username"),snv->msgHandler,&CSnMsgHandler::pastePassword);
        ac->setData(2);
        ac = ccm->addAction(tr("Insert password"),snv->msgHandler,&CSnMsgHandler::pastePassword);
        ac->setData(3);
        ccm->addSeparator();
        ac = ccm->addAction(QIcon::fromTheme(QSL("edit-delete")),
                            tr("Delete saved username and password"));
        connect(ac, &QAction::triggered, this, [origin](){
            gSet->removePassword(origin);
        });
    } else {
        ac = ccm->addAction(QIcon::fromTheme(QSL("edit-rename")),
                            tr("Save username and password"));
        connect(ac, &QAction::triggered, this, [origin](){
            QString realm = CGenericFuncs::elideString(gSet->cleanUrlForRealm(origin).toString(),
                                                       CDefaults::maxRealmLabelEliding,Qt::ElideLeft);
            CAuthDlg dlg(QApplication::activeWindow(),origin,realm,true);
            dlg.exec();
        });
    }
    m_menu.addSeparator();

    ccm = m_menu.addMenu(QIcon::fromTheme(QSL("system-run")),tr("Service"));
    ccm->addAction(QIcon::fromTheme(QSL("document-edit-verify")),
                   tr("Show source"),
                   this,&CSnCtxHandler::showSource,QKeySequence(Qt::CTRL + Qt::Key_E));

    ac = ccm->addAction(tr("Inspect page"));
    ac->setShortcut(QKeySequence(Qt::Key_F12));
    connect(ac, &QAction::triggered, snv->msgHandler, &CSnMsgHandler::showInspector);

    ccm->addSeparator();
    ccm->addAction(QIcon::fromTheme(QSL("documentation")),tr("Show in editor"),
                   this,&CSnCtxHandler::showInEditor);

    ac = ccm->addAction(QIcon::fromTheme(QSL("download")),tr("Open in browser"));
    ac->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
    connect(ac, &QAction::triggered,this,[this](){
        if (!QProcess::startDetached(gSet->settings()->sysBrowser,
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start browser."));
    });

    ac = ccm->addAction(QIcon::fromTheme(QSL("system-run")),
                     tr("Open with associated application"));
    connect(ac, &QAction::triggered,this,[this](){
        if (!QProcess::startDetached(QSL("xdg-open"),
                                     QStringList() << QString::fromUtf8(snv->getUrl().toEncoded())))
            QMessageBox::critical(snv, tr("JPReader"), tr("Unable to start associated application."));
    });

    ccm->addSeparator();

    ccm->addAction(QIcon::fromTheme(QSL("download-later")),tr("Download all images"),
                   snv->transHandler,&CSnTrans::getImgUrlsAndParse);

    ccm->addSeparator();

    ccm->addAction(QIcon::fromTheme(QSL("document-edit-decrypt")),tr("Parse document"),
                   snv->transHandler,&CSnTrans::reparseDocument);
    ccm->addSeparator();
    ccm->addAction(snv->parentWnd()->actionFullscreen);
    ccm->addAction(QIcon::fromTheme(QSL("dialog-close")),tr("Close tab"),
                   snv,&CSnippetViewer::closeTab);
    ccm->addSeparator();
    if (!sText.isEmpty()) {
        ac = ccm->addAction(QIcon::fromTheme(QSL("document-save-as")),
                            tr("Save selected text as file..."),
                            this,&CSnCtxHandler::saveToFile);
        ac->setData(sText);
    }
    ccm->addAction(QIcon::fromTheme(QSL("document-save")),tr("Save to file..."),
                   this,&CSnCtxHandler::saveToFile);
    ccm->addSeparator();
    ac = ccm->addAction(QIcon::fromTheme(QSL("preferences-web-browser-cookies")),tr("Export page cookies..."),
                   this,&CSnCtxHandler::exportCookies);
    ac->setData(origin);

    ccm = m_menu.addMenu(QIcon::fromTheme(QSL("document-edit-verify")),
                         tr("Translator options"));
    ccm->addActions(gSet->getTranslationLanguagesActions());

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

    p->action(QWebEnginePage::Paste)->setIcon(QIcon::fromTheme(QSL("edit-paste")));
    p->action(QWebEnginePage::SelectAll)->setIcon(QIcon::fromTheme(QSL("edit-select-all")));
    p->action(QWebEnginePage::OpenLinkInNewTab)->setIcon(QIcon::fromTheme(QSL("tab-new")));
    p->action(QWebEnginePage::OpenLinkInNewTab)->setText(tr("Open in new tab"));
    p->action(QWebEnginePage::OpenLinkInNewBackgroundTab)->
            setIcon(QIcon::fromTheme(QSL("tab-new")));
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
    auto *nt = qobject_cast<QAction *>(sender());
    if (nt==nullptr) return;
    QString s = nt->data().toString();
    if (s.isEmpty()) return;
    CLangPair lp(gSet->ui()->getActiveLangPair());
    if (!lp.isValid()) return;

    auto *th = new QThread();
    auto *at = new CAuxTranslator();
    at->setText(s);
    at->setSrcLang(lp.langFrom.bcp47Name());
    at->setDestLang(lp.langTo.bcp47Name());
    connect(th,&QThread::finished,at,&CAuxTranslator::deleteLater);
    connect(th,&QThread::finished,th,&QThread::deleteLater);

    connect(at,&CAuxTranslator::gotTranslation,this,[this](const QString& text){
        if (!text.isEmpty() && !isMenuActive()) {
            auto *lbl = new QLabel(CGenericFuncs::wordWrap(
                                      text,CDefaults::maxTranslateFragmentCharWidth));
            lbl->setStyleSheet(QSL("QLabel { background: #fefdeb; color: black; }"));
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
    auto *nt = qobject_cast<QAction *>(sender());
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
    AddBookmarkDialog dlg(lf->requestedUrl().toString(),lf->title(),snv);
    if (dlg.exec() == QDialog::Accepted)
        Q_EMIT gSet->updateAllBookmarks();
}

void CSnCtxHandler::showInEditor()
{
    QString fname = gSet->makeTmpFileName(QSL("html"),true);
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
    auto *srcv = new CSourceViewer(snv);
    srcv->showNormal();
}

void CSnCtxHandler::exportCookies()
{
    auto *ac = qobject_cast<QAction*>(sender());
    if (!ac) return;
    QUrl origin = ac->data().toUrl();
    if (!origin.isValid()) return;

    QString fname = CGenericFuncs::getSaveFileNameD(snv,tr("Save page cookies to file"),gSet->settings()->savedAuxSaveDir,
                                                    QStringList( { tr("Text file, Netscape format (*.txt)") } ) );

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

    if (!gSet->exportCookies(fname,origin))
        QMessageBox::critical(snv,tr("JPReader"),tr("Unable to create file."));
}

void CSnCtxHandler::runJavaScript()
{
    auto *nt = qobject_cast<QAction *>(sender());
    if (nt==nullptr) return;
    if (!nt->data().canConvert<QString>()) return;

    const QString source = nt->data().toString();

    if (!source.isEmpty())
        snv->txtBrowser->page()->runJavaScript(source,QWebEngineScript::MainWorld);
}
