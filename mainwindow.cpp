#include <QShortcut>
#include <QScreen>
#include <QWindow>
#include <QMessageBox>
#include <QTextCodec>
#include <QAbstractNetworkCache>
#include <QTextDocument>
#include <QWebEngineSettings>
#include <QMimeData>

#include "qxttooltip.h"
#include "mainwindow.h"
#include "snviewer.h"
#include "settingsdlg.h"
#include "genericfuncs.h"
#include "globalcontrol.h"
#include "specwidgets.h"
#include "searchtab.h"
#include "lighttranslator.h"
#include "downloadmanager.h"

#include "snctxhandler.h"

CMainWindow::CMainWindow(bool withSearch, bool withViewer, const QVector<QUrl> &viewerUrls)
        : MainWindow()
{
	setupUi(this);

    tabMain->parentWnd=this;
	lastTabIdx=0;
    setWindowIcon(gSet->appIcon);
    setAttribute(Qt::WA_DeleteOnClose,true);

    tabMain->tabBar()->setBrowserTabs(true);

    stSearchStatus.setText(tr("Ready"));
    stSearchStatus.setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    statusBar()->addPermanentWidget(&stSearchStatus);

    savedHelperWidth=0;
    savedHelperIdx=-1;
    helperVisible = false;
    tabHelper = new CSpecTabBar(this);
    tabHelper->setShape(QTabBar::RoundedWest);

    //splitter->insertWidget(0,tabHelper);
    auto hbx = qobject_cast<QHBoxLayout *>(centralWidget()->layout());
    if (hbx)
        hbx->insertWidget(0,tabHelper,0,Qt::AlignTop);
    else
        qCritical() << "Main HBox layout not found. Check form design, update source.";
    tabHelper->addTab(tr("Tabs"));
    tabHelper->addTab(tr("Recycled"));
    tabHelper->addTab(tr("History"));
    updateSplitters();

    recentMenu = new QMenu(menuFile);
    actionRecentDocuments->setMenu(recentMenu);

    menuBar()->addSeparator();
    tabsMenu = menuBar()->addMenu(QIcon::fromTheme(QStringLiteral("tab-detach")),QString());
    recycledMenu = menuBar()->addMenu(QIcon::fromTheme(QStringLiteral("user-trash")),QString());

    menuBookmarks->setStyleSheet(QStringLiteral("QMenu { menu-scrollable: 1; }"));
    menuBookmarks->setToolTipsVisible(true);

    titleRenamedLock.setInterval(500);
    titleRenamedLock.setSingleShot(true);

    connect(actionAbout, &QAction::triggered, this, &CMainWindow::helpAbout);
    connect(actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(actionSettings, &QAction::triggered, &(gSet->settings), &CSettings::settingsDlg);
    connect(actionExit, &QAction::triggered, this, &CMainWindow::close);
    connect(actionExitAll, &QAction::triggered, gSet, &CGlobalControl::cleanupAndExit);
    connect(actionOpen, &QAction::triggered, this, &CMainWindow::openAuxFileWithDialog);
    connect(actionNew, &QAction::triggered, this, &CMainWindow::openEmptyBrowser);
    connect(actionOpenInDir, &QAction::triggered, this, &CMainWindow::openAuxFileInDir);
    connect(actionOpenCB, &QAction::triggered, this, &CMainWindow::createFromClipboard);
    connect(actionOpenCBPlain, &QAction::triggered, this, &CMainWindow::createFromClipboardPlain);
    connect(actionSave, &QAction::triggered, this, &CMainWindow::save);
    connect(actionClearCB, &QAction::triggered, this, &CMainWindow::clearClipboard);
    connect(actionClearCache, &QAction::triggered, gSet, &CGlobalControl::clearCaches);
    connect(actionOpenClip, &QAction::triggered, this, &CMainWindow::openFromClipboard);
    connect(actionWnd, &QAction::triggered, &(gSet->ui), &CGlobalUI::addMainWindow);
    connect(actionNewSearch, &QAction::triggered, this, &CMainWindow::createSearch);
    connect(actionAddBM, &QAction::triggered, this, &CMainWindow::addBookmark);
    connect(actionManageBM, &QAction::triggered, this, &CMainWindow::manageBookmarks);
    connect(actionTextTranslator, &QAction::triggered, this, &CMainWindow::showLightTranslator);
    connect(actionDetachTab, &QAction::triggered, this, &CMainWindow::detachTab);
    connect(actionFindText, &QAction::triggered, this, &CMainWindow::findText);
    connect(actionDictionary, &QAction::triggered, gSet, &CGlobalControl::showDictionaryWindow);
    connect(actionFullscreen, &QAction::triggered, this, &CMainWindow::switchFullscreen);
    connect(actionInspector, &QAction::triggered, this, &CMainWindow::openBookmark);
    connect(actionPrintPDF, &QAction::triggered, this, &CMainWindow::printToPDF);
    connect(actionSaveSettings,&QAction::triggered, &(gSet->settings), &CSettings::writeSettings);
    connect(tabMain, &CSpecTabWidget::currentChanged, this, &CMainWindow::tabChanged);
    connect(tabHelper, &CSpecTabBar::tabLeftPostClicked, this, &CMainWindow::helperClicked);
    connect(tabHelper, &CSpecTabBar::tabLeftClicked, this, &CMainWindow::helperPreClicked);
    connect(helperList, &QListWidget::currentItemChanged, this, &CMainWindow::helperItemClicked);
    connect(splitter, &QSplitter::splitterMoved, this, &CMainWindow::splitterMoved);
    connect(tabMain, &CSpecTabWidget::tooltipRequested, this, &CMainWindow::tabBarTooltip);

    actionInspector->setData(gSet->getInspectorUrl());

    if (gSet->logWindow)
        connect(actionShowLog, &QAction::triggered, gSet->logWindow, &CLogDisplay::show);
    if (gSet->downloadManager)
        connect(actionDownloadManager, &QAction::triggered,
                gSet->downloadManager, &CDownloadManager::show);

    QShortcut* sc;
    sc = new QShortcut(QKeySequence(Qt::CTRL+Qt::Key_Left),this);
    sc->setContext(Qt::WindowShortcut);
    connect(sc, &QShortcut::activated, tabMain, &CSpecTabWidget::selectPrevTab);
    sc = new QShortcut(QKeySequence(Qt::CTRL+Qt::Key_Right),this);
    sc->setContext(Qt::WindowShortcut);
    connect(sc, &QShortcut::activated, tabMain, &CSpecTabWidget::selectNextTab);

    centerWindow();

    reloadCharsetList();
    updateBookmarks();
    updateQueryHistory();
    updateRecycled();
    updateRecentList();
    reloadLanguagesList();

    if (withSearch) createSearch();
    if (withViewer) {
        bool emptyViewer = true;
        for (const auto &url : viewerUrls) {
            if (!url.isEmpty()) {
                new CSnippetViewer(this,url);
                emptyViewer = false;
            }
        }
        if (emptyViewer)
            createStartBrowser();
    }

    fullScreen=false;
    savedPos=pos();
    savedSize=size();
    savedMaximized=isMaximized();
    savedSplitterWidth=splitter->handleWidth();

    qApp->installEventFilter(this);

    QPushButton* addTabButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")),QString());
    addTabButton->setFlat(true);
    tabMain->setCornerWidget(addTabButton);
    connect(addTabButton,&QPushButton::clicked,actionNew,&QAction::trigger);

    setAcceptDrops(true);
}

void CMainWindow::updateSplitters()
{
    splitter->setCollapsible(0,true);
    if (savedHelperWidth<150) savedHelperWidth=150;

    QList<int> widths;
    if (helperVisible)
        widths << savedHelperWidth;
    else
        widths << 0;
    if (!helperVisible && helperFrame->width()>0)
        savedHelperWidth=helperFrame->width();

    widths << splitter->width()-widths[0];
    splitter->setSizes(widths);
}

void CMainWindow::closeStartPage()
{
    CSnippetViewer* pt = nullptr;
    for (int i=0;i<tabMain->count();i++) {
        auto sn = qobject_cast<CSnippetViewer *>(tabMain->widget(i));
        if (sn==nullptr) continue;
        if (!sn->isStartPage) continue;
        pt = sn;
        break;
    }
    if (pt)
        pt->closeTab(true);
}

void CMainWindow::tabBarTooltip(const QPoint &globalPos, const QPoint &)
{
    QPoint p = tabMain->tabBar()->mapFromGlobal(globalPos);
    int idx = tabMain->tabBar()->tabAt(p);
    if (idx<0) return;

    CSpecToolTipLabel* t = nullptr;

    auto sv = qobject_cast<CSnippetViewer *>(tabMain->widget(idx));
    if (sv) {
        t = new CSpecToolTipLabel(sv->tabTitle);

        QPixmap pix = sv->pageImage;
        bool fillRect = false;
        if (pix.isNull()) {
            pix = QPixmap(250,100);
            fillRect = true;
        } else
            pix = pix.scaledToWidth(250,Qt::SmoothTransformation);
        QPainter dc(&pix);
        QRect rx = pix.rect();
        QFont f = dc.font();
        f.setPointSize(8);
        dc.setFont(f);
        if (fillRect)
            dc.fillRect(rx,t->palette().window());
        rx.setHeight(dc.fontMetrics().height());
        dc.fillRect(rx,t->palette().toolTipBase());
        dc.setPen(t->palette().toolTipText().color());
        dc.drawText(rx,Qt::AlignLeft | Qt::AlignVCenter,sv->tabTitle);

        t->setPixmap(pix);
    } else {
        auto bt = qobject_cast<CSearchTab *>(tabMain->widget(idx));
        if (bt) {
            t = new CSpecToolTipLabel(tr("<b>Search:</b> %1").arg(bt->getLastQuery()));
        }
    }

    if (t)
        QxtToolTip::show(globalPos,t,tabMain->tabBar());
}

void CMainWindow::detachTab()
{
    if (tabMain->currentWidget()) {
        auto bt = qobject_cast<CSpecTabContainer *>(tabMain->currentWidget());
        if (bt)
            bt->detachTab();
    }
}

void CMainWindow::findText()
{
    if (tabMain->currentWidget()) {
        auto sn = qobject_cast<CSnippetViewer *>(tabMain->currentWidget());
        if (sn)
            sn->searchPanel->show();
    }
}

void CMainWindow::switchFullscreen()
{
    fullScreen = !fullScreen;

    if (fullScreen) {
        savedMaximized=isMaximized();
        savedPos=pos();
        savedSize=size();
        savedSplitterWidth=splitter->handleWidth();
    }

    statusBar()->setVisible(!fullScreen);
    helperFrame->setVisible(!fullScreen);
    tabHelper->setVisible(!fullScreen);
    splitter->setHandleWidth(fullScreen ? 0 : savedSplitterWidth);
    if (splitter->handle(0))
        splitter->handle(0)->setVisible(!fullScreen);

    setToolsVisibility(!fullScreen);

    if (fullScreen)
        showFullScreen();
    else {
        if (savedMaximized)
            showMaximized();
        else {
            showNormal();
            move(savedPos);
            resize(savedSize);
        }
    }
    actionFullscreen->setChecked(fullScreen);
}

void CMainWindow::setToolsVisibility(bool visible)
{
    menuBar()->setVisible(visible);
    tabMain->tabBar()->setVisible(visible);
    for (int i=0;i<tabMain->count();i++) {
        auto sc = qobject_cast<CSpecTabContainer *>(tabMain->widget(i));
        if (sc)
            sc->setToolbarVisibility(visible);
    }
}

void CMainWindow::printToPDF()
{
    if (tabMain->currentWidget()==nullptr) return;
    auto sn = qobject_cast<CSnippetViewer *>(tabMain->currentWidget());
    if (sn==nullptr) return;

    sn->printToPDF();
}

void CMainWindow::save()
{
    if (tabMain->currentWidget()==nullptr) return;
    auto sv = qobject_cast<CSnippetViewer *>(tabMain->currentWidget());
    if (sv==nullptr) return;

    sv->ctxHandler->saveToFile();
}

void CMainWindow::splitterMoved(int, int)
{
    helperVisible=(helperFrame->width()>0);
}

void CMainWindow::centerWindow()
{
    QScreen *screen = nullptr;

    if (window() && window()->windowHandle()) {
        screen = window()->windowHandle()->screen();
    } else if (!QApplication::screens().isEmpty()) {
        screen = QApplication::screenAt(QCursor::pos());
    }
    if (screen == nullptr)
        screen = QApplication::primaryScreen();

    QRect rect(screen->availableGeometry());
	int h = 80*rect.height()/100;
    QSize nw(135*h/100,h);
    if (nw.width()<1000) nw.setWidth(80*rect.width()/100);
    resize(nw);
    if (gSet->mainWindows.count()==0) {
        move(rect.width()/2 - frameGeometry().width()/2,
             rect.height()/2 - frameGeometry().height()/2);
    } else {
        CMainWindow* w = gSet->mainWindows.constLast();
        if (QApplication::activeWindow())
            if (qobject_cast<CMainWindow *>(QApplication::activeWindow()))
                w = qobject_cast<CMainWindow *>(QApplication::activeWindow());
        move(w->pos().x()+25,w->pos().y()+25);
    }
}

void CMainWindow::updateQueryHistory()
{
    for(int i=0;i<tabMain->count();i++) {
        auto t = qobject_cast<CSearchTab*>(tabMain->widget(i));
        if (t)
            t->updateQueryHistory();
    }
}

void CMainWindow::helperPreClicked(int) {
    savedHelperIdx = tabHelper->currentIndex();
}

void CMainWindow::helperClicked(int index)
{
    if (index<0 || index>=tabHelper->count()) return;
    if (helperVisible && savedHelperIdx==tabHelper->currentIndex())
        helperVisible=false;
    else if (!helperVisible)
        helperVisible=true;
    updateSplitters();
    updateHelperList();
}

void CMainWindow::tabChanged(int idx)
{
    lastTabIdx=idx;

    tabMain->tabBar()->setTabTextColor(idx,QApplication::palette(tabMain->tabBar()).windowText().color());
    if (tabMain->widget(idx)) {
        auto sn = qobject_cast<CSnippetViewer *>(tabMain->widget(idx));
        if (sn) {
            sn->translationBkgdFinished=false;
            sn->loadingBkgdFinished=false;
            sn->txtBrowser->setFocus(Qt::OtherFocusReason);
            sn->takeScreenshot();
        }
    }

    updateTitle();
    updateHelperList();

    if (tabMain->currentWidget()) {
        auto bt = qobject_cast<CSnippetViewer *>(tabMain->currentWidget());
        actionAddBM->setEnabled(bt);
    }
}

CSnippetViewer* CMainWindow::getOpenedInspectorTab()
{
    for (int i=0;i<tabMain->count();i++) {
        auto sv = qobject_cast<CSnippetViewer*>(tabMain->widget(i));
        if (sv==nullptr) continue;

        if (sv->isInspector())
            return sv;
    }
    return nullptr;
}

void CMainWindow::updateHelperList()
{
    helperList->clear();
    switch (tabHelper->currentIndex()) {
        case 0: // Tabs
            for (int i=0;i<tabMain->count();i++) {
                QListWidgetItem* it = new QListWidgetItem(tr("Tab %1").arg(i));
                it->setData(Qt::UserRole,0);
                it->setData(Qt::UserRole+1,i);
                auto sv = qobject_cast<CSnippetViewer*>(tabMain->widget(i));
                if (sv) {
                    it->setText(sv->tabTitle);
                    it->setStatusTip(sv->getUrl().toString());
                    it->setToolTip(sv->getUrl().toString());
                    if (sv->translationBkgdFinished)
                        it->setForeground(QBrush(Qt::green));
                    else if (sv->loadingBkgdFinished)
                        it->setForeground(QBrush(Qt::blue));
                    else
                        it->setForeground(QApplication::palette(helperList).text());
                }
                auto bv = qobject_cast<CSearchTab*>(tabMain->widget(i));
                if (bv) {
                    QString qr = bv->getLastQuery();
                    if (qr.isEmpty()) qr = tr("(empty)");
                    it->setText(tr("Search: %1").arg(qr));
                    it->setToolTip(qr);
                }
                helperList->addItem(it);
            }
            break;
        case 1: // Recycled
            for (int i=0;i<gSet->recycleBin.count();i++) {
                auto it = new QListWidgetItem(gSet->recycleBin.at(i).getTitle());
                it->setData(Qt::UserRole,1);
                it->setData(Qt::UserRole+1,i);
                helperList->addItem(it);
            }
            break;
        case 2: // History
            for (const CUrlHolder &t : qAsConst(gSet->mainHistory)) {
                auto it = new QListWidgetItem(t.getTitle());
                it->setStatusTip(t.getUrl().toString());
                it->setToolTip(t.getUrl().toString());
                it->setData(Qt::UserRole,2);
                it->setData(Qt::UserRole+1,0);
                it->setData(Qt::UserRole+2,t.getUuid().toString());
                helperList->addItem(it);
            }
            break;
    }
}

void CMainWindow::helperItemClicked(QListWidgetItem *current, QListWidgetItem *)
{
    if (current==nullptr) return;
    bool okconv;
    int idx = current->data(Qt::UserRole+1).toInt(&okconv);
    if (!okconv) return;
    int t = current->data(Qt::UserRole).toInt(&okconv);
    if (!okconv) return;
    QUrl u;
    switch (t) {
        case 0: // Tabs
            if (idx<0 || idx>=tabMain->count()) return;
            tabMain->setCurrentIndex(idx);
            break;
        case 1: // Recycled
            if (idx<0 || idx>=gSet->recycleBin.count()) return;
            u = gSet->recycleBin.at(idx).getUrl();
            if (!u.isValid()) return;
            new CSnippetViewer(this, u);

            gSet->recycleBin.removeAt(idx);
            updateRecycled();
            break;
        case 2: // History
            QUuid uidx(current->data(Qt::UserRole+2).toString());
            if (!uidx.isNull()) goHistory(uidx);
            break;
    }
}

void CMainWindow::updateHistoryList()
{
    if (!(helperVisible && tabHelper->currentIndex()==2)) return;
    helperList->clear();
    for (const CUrlHolder &t : qAsConst(gSet->mainHistory)) {
        auto it = new QListWidgetItem(t.getTitle());
        it->setStatusTip(t.getUrl().toString());
        it->setToolTip(t.getUrl().toString());
        it->setData(Qt::UserRole,2);
        it->setData(Qt::UserRole+1,0);
        it->setData(Qt::UserRole+2,t.getUuid().toString());
        helperList->addItem(it);
    }
}

void CMainWindow::updateRecentList()
{
    recentMenu->clear();
    actionRecentDocuments->setEnabled(gSet->settings.maxRecent>0);

    for(const QString& filename : qAsConst(gSet->recentFiles)) {
        QFileInfo fi(filename);
        auto ac = recentMenu->addAction(fi.fileName());
        ac->setToolTip(filename);
        connect(ac,&QAction::triggered,this,[this,filename](){
            openAuxFiles(QStringList() << filename);
        });
    }
}

void CMainWindow::updateTitle()
{
    QString t = tr("JPReader");
    if (tabMain->currentWidget()) {
        auto sv = qobject_cast<CSnippetViewer*>(tabMain->currentWidget());
        if (sv&& !sv->tabTitle.isEmpty()) {
            QTextDocument doc;
            doc.setHtml(sv->tabTitle);
            t = QStringLiteral("%1 - %2").arg(doc.toPlainText(), t);
            t.remove('\r');
            t.remove('\n');
        }
        auto bv = qobject_cast<CSearchTab*>(tabMain->currentWidget());
        if (bv && !bv->getLastQuery().isEmpty()) t =
                tr("[%1] search - %2").arg(bv->getLastQuery(),t);
    }
    setWindowTitle(t);
}

void CMainWindow::goHistory(QUuid idx)
{
    for (const CUrlHolder& uh : qAsConst(gSet->mainHistory))
        if (uh.getUuid()==idx) {
            QUrl u = uh.getUrl();
            if (!u.isValid()) return;
            new CSnippetViewer(this, u);
            break;
        }
}

void CMainWindow::createSearch()
{
    new CSearchTab(this);
}

void CMainWindow::createStartBrowser()
{
    QFile f(QStringLiteral(":/data/startpage"));
    QString html;
    if (f.open(QIODevice::ReadOnly))
        html = QString::fromUtf8(f.readAll());
    new CSnippetViewer(this,QUrl(),QStringList(),true,html,QStringLiteral("100%"),true);
}

void CMainWindow::checkTabs()
{
    if(tabMain->count()==0) close();
}

void CMainWindow::openAuxFiles(const QStringList &filenames)
{
    for(const QString& fname : qAsConst(filenames))
        if (!fname.isEmpty())
            new CSnippetViewer(this, QUrl::fromLocalFile(fname));
}

void CMainWindow::openAuxFileWithDialog()
{
    QStringList fnames = getOpenFileNamesD(this,tr("Open text file"),gSet->settings.savedAuxDir);
    if (fnames.isEmpty()) return;

    gSet->settings.savedAuxDir = QFileInfo(fnames.first()).absolutePath();

    openAuxFiles(fnames);
}

void CMainWindow::openEmptyBrowser()
{
    CSnippetViewer* sv = new CSnippetViewer(this);
    sv->urlEdit->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::openAuxFileInDir()
{
    QWidget* t = tabMain->currentWidget();
    auto sv = qobject_cast<CSnippetViewer *>(t);
    if (sv==nullptr) {
        QMessageBox::warning(this,tr("JPReader"),
                             tr("Active document viewer tab not found."));
        return;
    }
    QString auxDir = sv->getUrl().toLocalFile();
    if (auxDir.isEmpty()) {
        QMessageBox::warning(this,tr("JPReader"),tr("Remote document opened. Cannot define local directory."));
        return;
    }
    auxDir = QFileInfo(auxDir).absolutePath();

    QStringList fnames = getOpenFileNamesD(this,tr("Open text file"),auxDir);
    if (fnames.isEmpty()) return;

    gSet->settings.savedAuxDir = QFileInfo(fnames.first()).absolutePath();

    for(int i=0;i<fnames.count();i++) {
        if (fnames.at(i).isEmpty()) continue;
        new CSnippetViewer(this, QUrl::fromLocalFile(fnames.at(i)));
    }
}

void CMainWindow::createFromClipboard()
{
    QString tx = getClipboardContent();
    if (tx.isEmpty()) {
        QMessageBox::information(this, tr("JPReader"),tr("Clipboard is empty or contains incompatible data."));
        return;
    }
    CSnippetViewer* sv = new CSnippetViewer(this, QUrl(), QStringList(), true, tx);
    sv->txtBrowser->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::createFromClipboardPlain()
{
    QString tx = getClipboardContent(true,true);
    if (tx.isEmpty()) {
        QMessageBox::information(this, tr("JPReader"),tr("Clipboard is empty or contains incompatible data."));
        return;
    }
    CSnippetViewer* sv = new CSnippetViewer(this, QUrl(), QStringList(), true, tx);
    sv->txtBrowser->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::clearClipboard()
{
    QApplication::clipboard()->clear(QClipboard::Clipboard);
}

void CMainWindow::openFromClipboard()
{
    QUrl url = QUrl::fromUserInput(getClipboardContent(true));
    url.setFragment(QString());
    QString uri = url.toString().remove(QRegExp("#$"));
    if (uri.isEmpty()) {
        QMessageBox::information(this, tr("JPReader"),tr("Clipboard is empty or contains incompatible data."));
        return;
    }
    CSnippetViewer* sv = new CSnippetViewer(this, uri);
    sv->txtBrowser->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::updateBookmarks()
{
    while (true) {
        const QList<QAction *> acl = menuBookmarks->actions();
        if (acl.count()>3)
            menuBookmarks->removeAction(acl.last());
        else
            break;
    }

    gSet->bookmarksManager->populateBookmarksMenu(menuBookmarks, this);
}

void CMainWindow::addBookmark()
{
    if (tabMain->currentWidget()==nullptr) return;
    auto sv = qobject_cast<CSnippetViewer *>(tabMain->currentWidget());
    if (sv==nullptr) return;

    auto dlg = new AddBookmarkDialog(sv->getUrl().toString(),sv->tabTitle,this);
    if (dlg->exec())
        emit gSet->updateAllBookmarks();

    dlg->setParent(nullptr);
    delete dlg;
}

void CMainWindow::manageBookmarks()
{
    auto dialog = new BookmarksDialog(this);
    connect(dialog, &BookmarksDialog::openUrl,this,[this](const QUrl& url){
        new CSnippetViewer(this, url);
    });

    dialog->exec();
    emit gSet->updateAllBookmarks();
}

void CMainWindow::showLightTranslator()
{
    gSet->showLightTranslator();
}

void CMainWindow::openBookmark()
{
    auto a = qobject_cast<QAction *>(sender());
    if (a==nullptr) return;

    if (a->data().canConvert<QUrl>()) {
        QUrl u = a->data().toUrl();
        if (!u.isValid()) {
            QMessageBox::warning(this,tr("JPReader"),
                                 tr("Unable to open inconsistently loaded bookmark."));
            return;
        }

        new CSnippetViewer(this, u);
    } else if (a->data().canConvert<QStringList>()) {
        QStringList sl = a->data().toStringList();
        for (const QString &s : qAsConst(sl)) {
            QUrl u(s);
            if (u.isValid())
                new CSnippetViewer(this, u);
        }
    }
}

void CMainWindow::openRecycled()
{
    auto a = qobject_cast<QAction *>(sender());
    if (a==nullptr) return;

    bool okconv;
    int idx = a->data().toInt(&okconv);
    if (!okconv) return;
    if (idx<0 || idx>=gSet->recycleBin.count()) return;
    QUrl u = gSet->recycleBin.at(idx).getUrl();
    if (!u.isValid()) return;
    new CSnippetViewer(this, u);

    gSet->recycleBin.removeAt(idx);
    updateRecycled();
}

void CMainWindow::updateRecycled()
{
    recycledMenu->clear();
    for (int i=0;i<gSet->recycleBin.count();i++) {
        QAction* a = recycledMenu->addAction(gSet->recycleBin.at(i).getTitle(),this,
                                             &CMainWindow::openRecycled);
        a->setStatusTip(gSet->recycleBin.at(i).getUrl().toString());
        a->setData(i);
    }
    updateHelperList();
}

void CMainWindow::updateTabs()
{
    tabsMenu->clear();
    for(int i=0;i<tabMain->count();i++) {
        auto sv = qobject_cast<CSpecTabContainer*>(tabMain->widget(i));
        if (sv)
            tabsMenu->addAction(sv->getDocTitle(),this,
                                &CMainWindow::activateTab)->setData(i);
    }
    updateHelperList();
}

void CMainWindow::activateTab()
{
    auto act = qobject_cast<QAction*>(sender());
    if(act==nullptr) return;
    bool okconv;
    int idx = act->data().toInt(&okconv);
    if (!okconv) return;
    tabMain->setCurrentIndex(idx);
}

void CMainWindow::forceCharset()
{
    auto act = qobject_cast<QAction*>(sender());
    if (act==nullptr) return;

    QString cs = act->data().toString();
    if (!cs.isEmpty()) {
        if (QTextCodec::codecForName(cs.toLatin1().data()))
            cs = QTextCodec::codecForName(cs.toLatin1().data())->name();

        gSet->settings.charsetHistory.removeAll(cs);
        gSet->settings.charsetHistory.prepend(cs);
        if (gSet->settings.charsetHistory.count()>10)
            gSet->settings.charsetHistory.removeLast();
    }
    gSet->settings.forcedCharset = cs;
    emit gSet->updateAllCharsetLists();

    if (gSet->webProfile &&
            gSet->webProfile->settings())
        gSet->webProfile->settings()->setDefaultTextEncoding(cs);
}

void CMainWindow::reloadLanguagesList()
{
    menuTranslationLanguages->clear();
    menuTranslationLanguages->addActions(gSet->ui.languageSelector->actions());
}

void CMainWindow::reloadCharsetList()
{
    QAction* act;
    menuCharset->clear();
    act = menuCharset->addAction(tr("Autodetect"),this,&CMainWindow::forceCharset);
    act->setData(QString());
    if (gSet->settings.forcedCharset.isEmpty()) {
        act->setCheckable(true);
        act->setChecked(true);
    } else {
        act = menuCharset->addAction(gSet->settings.forcedCharset,this,&CMainWindow::forceCharset);
        act->setData(gSet->settings.forcedCharset);
        act->setCheckable(true);
        act->setChecked(true);
    }
    menuCharset->addSeparator();

    QVector<QStringList> cList = encodingsByScript();
    for(int i=0;i<cList.count();i++) {
        QMenu* midx = menuCharset->addMenu(cList.at(i).at(0));
        for(int j=1;j<cList.at(i).count();j++) {
            if (QTextCodec::codecForName(cList.at(i).at(j).toLatin1())==nullptr) {
                qWarning() << tr("Encoding %1 not supported.").arg(cList.at(i).at(j));
                continue;
            }
            QString cname = QTextCodec::codecForName(cList.at(i).at(j).toLatin1())->name();
            act = midx->addAction(cname,this,&CMainWindow::forceCharset);
            act->setData(cname);
            if (QTextCodec::codecForName(cname.toLatin1().data())) {
                if (QTextCodec::codecForName(cname.toLatin1().data())->name() ==
                        gSet->settings.forcedCharset) {
                    act->setCheckable(true);
                    act->setChecked(true);
                }
            }
        }
    }
    menuCharset->addSeparator();

    for(int i=0;i<gSet->settings.charsetHistory.count();i++) {
        if (gSet->settings.charsetHistory.at(i)==gSet->settings.forcedCharset) continue;
        act = menuCharset->addAction(gSet->settings.charsetHistory.at(i),this,
                                     &CMainWindow::forceCharset);
        act->setData(gSet->settings.charsetHistory.at(i));
        act->setCheckable(true);
    }
}

void CMainWindow::closeEvent(QCloseEvent *event)
{
    emit aboutToClose(this);
    event->accept();
}

bool CMainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    QString cln(obj->metaObject()->className());
    if (fullScreen && cln.contains(QStringLiteral("WebEngine"),Qt::CaseInsensitive)) {
        if (ev->type()==QEvent::MouseMove) {
            auto mev = dynamic_cast<QMouseEvent *>(ev);
            if (mev) {
                if (mev->y()<20 && !tabMain->tabBar()->isVisible())
                    setToolsVisibility(true);
                else if (mev->y()>25 && tabMain->tabBar()->isVisible())
                    setToolsVisibility(false);
            }
        }
    }
    return QMainWindow::eventFilter(obj,ev);
}

void CMainWindow::dragEnterEvent(QDragEnterEvent *ev)
{
    QStringList formats = ev->mimeData()->formats();

    if (formats.contains(QStringLiteral("_NETSCAPE_URL")) ||
            formats.contains(QStringLiteral("text/uri-list")))
        ev->acceptProposedAction();
    else if (formats.contains(QStringLiteral("text/plain"))) {
        QUrl u(ev->mimeData()->text());
        if (u.isValid())
            ev->acceptProposedAction();
    }
}

void CMainWindow::dropEvent(QDropEvent *ev)
{
    QHash<QString,QString> data;
    QStringList formats = ev->mimeData()->formats();
    for (int i=0;i<formats.count();i++)
        data[formats.at(i)] = detectDecodeToUnicode(ev->mimeData()->data(formats.at(i)));

    QVector<QUrl> ul;
    bool ok = false;

    if (data.contains(QStringLiteral("_NETSCAPE_URL"))) {
        QString s = data.value(QStringLiteral("_NETSCAPE_URL"))
                               .split(QRegExp(QStringLiteral("\n|\r\n|\r"))).constFirst();
        QUrl u(s);
        if (u.isValid()) {
            ul << u;
            ok = true;
        }
    }
    if (!ok && data.contains(QStringLiteral("text/uri-list"))) {
        QStringList sl = data.value(QStringLiteral("text/uri-list"))
                         .split(QRegExp(QStringLiteral("\n|\r\n|\r")));
        for (int i=0;i<sl.count();i++) {
            if (sl.at(i).startsWith('#')) continue;
            QUrl u(sl.at(i));
            if (u.isValid()) {
                ul << u;
                ok = true;
            }
        }
    }
    if (!ok && data.contains(QStringLiteral("text/plain"))) {
        QUrl u(data.value(QStringLiteral("text/plain"))
               .split(QRegExp(QStringLiteral("\n|\r\n|\r"))).constFirst());
        if (u.isValid())
            ul << u;
    }

    for (const QUrl& u : qAsConst(ul)) {
        auto sv = new CSnippetViewer(this, u);
        sv->txtBrowser->setFocus(Qt::OtherFocusReason);
    }
}

void CMainWindow::helpAbout()
{
    QString recoll = tr("no");
    QString baloo5 = tr("no");
    QString poppler = tr("no");
    QString debugstr;
    debugstr.clear();
#ifdef QT_DEBUG
    debugstr = tr ("(debug)\n");
#endif
#ifdef WITH_RECOLL
    recoll = tr("yes");
#endif
#ifdef WITH_BALOO5
    baloo5 = tr("yes");
#endif
#ifdef WITH_POPPLER
    poppler = tr("yes");
#endif
    QString msg = tr("JPReader for searching, translating and reading text files in Japanese\n\n"
                     "Build: %1 %2\n"
                     "Platform: %3\n"
                     "Build date: %4\n\n"
                     "Recoll backend compiled: %5\n"
                     "Baloo backend compiled: %6\n"
                     "Poppler support: %7.")
                    .arg(QStringLiteral(BUILD_REV),
                    debugstr,
                    QStringLiteral(BUILD_PLATFORM),
                    QStringLiteral(BUILD_DATE),
                    recoll,
                    baloo5,
                    poppler);

    QMessageBox::about(this, tr("JPReader"),msg);
}
