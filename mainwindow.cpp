#include <QShortcut>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QTextCodec>
#include <QAbstractNetworkCache>
#include <QTextDocument>
#include <QWebEngineSettings>

#include "qxttooltip.h"
#include "mainwindow.h"
#include "snviewer.h"
#include "settingsdlg.h"
#include "genericfuncs.h"
#include "globalcontrol.h"
#include "specwidgets.h"
#include "searchtab.h"
#include "bookmarkdlg.h"
#include "lighttranslator.h"
#include "downloadmanager.h"

CMainWindow::CMainWindow(bool withSearch, bool withViewer)
        : MainWindow()
{
	setupUi(this);

    tabMain->parentWnd=this;
	lastTabIdx=0;
	savedTabIdx=0;
    savedHelperWidth=0;
    setWindowIcon(gSet->appIcon);

    tabMain->tabBar()->setBrowserTabs(true);

    stSearchStatus.setText(tr("Ready"));
    stSearchStatus.setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    statusBar()->addPermanentWidget(&stSearchStatus);

    helperVisible = false;
    tabHelper = new CSpecTabBar(this);
    tabHelper->setShape(QTabBar::RoundedWest);

    //splitter->insertWidget(0,tabHelper);
    QHBoxLayout *hbx = qobject_cast<QHBoxLayout *>(centralWidget()->layout());
    if (hbx!=NULL)
        hbx->insertWidget(0,tabHelper,0,Qt::AlignTop);
    else
        qCritical() << "Main HBox layout not found. Check form design, update source.";
    tabHelper->addTab(tr("Tabs"));
    tabHelper->addTab(tr("Recycled"));
    tabHelper->addTab(tr("History"));
    updateSplitters();

	actionExit->setIcon(QIcon::fromTheme("application-exit"));
    actionExitAll->setIcon(QIcon::fromTheme("application-exit"));
    actionSettings->setIcon(QIcon::fromTheme("configure"));
	actionAbout->setIcon(QIcon::fromTheme("help-about"));
    actionOpen->setIcon(QIcon::fromTheme("document-open"));
    actionOpenInDir->setIcon(QIcon::fromTheme("document-open-folder"));
    actionNew->setIcon(QIcon::fromTheme("document-new"));
    actionOpenCB->setIcon(QIcon::fromTheme("edit-paste"));
    actionClearCB->setIcon(QIcon::fromTheme("edit-clear"));
    actionOpenClip->setIcon(QIcon::fromTheme("document-open-remote"));
    actionWnd->setIcon(QIcon::fromTheme("window-new"));
    actionTextTranslator->setIcon(QIcon::fromTheme("document-edit-verify"));

    menuBar()->addSeparator();
	tabsMenu = menuBar()->addMenu(QIcon::fromTheme("tab-detach"),"");
	recycledMenu = menuBar()->addMenu(QIcon::fromTheme("user-trash"),"");

	connect(actionAbout, SIGNAL(triggered()), this, SLOT(helpAbout()));
    connect(actionAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(actionSettings, SIGNAL(triggered()), gSet, SLOT(settingsDlg()));
	connect(actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(actionExitAll, SIGNAL(triggered()), gSet, SLOT(cleanupAndExit()));
    connect(actionOpen, SIGNAL(triggered()), this, SLOT(openAuxFile()));
    connect(actionNew, SIGNAL(triggered()), this, SLOT(openEmptyBrowser()));
    connect(actionOpenInDir, SIGNAL(triggered()), this, SLOT(openAuxFileInDir()));
    connect(actionOpenCB, SIGNAL(triggered()), this, SLOT(createFromClipboard()));
    connect(actionOpenCBPlain, SIGNAL(triggered()), this, SLOT(createFromClipboardPlain()));
    connect(actionClearCB, SIGNAL(triggered()), this, SLOT(clearClipboard()));
    connect(actionOpenClip, SIGNAL(triggered()), this, SLOT(openFromClipboard()));
    connect(actionWnd, SIGNAL(triggered()), gSet, SLOT(addMainWindow()));
    connect(actionNewSearch, SIGNAL(triggered()), this, SLOT(createSearch()));
    connect(actionSaveSettings,SIGNAL(triggered()), gSet, SLOT(writeSettings()));
    connect(actionAddBM,SIGNAL(triggered()),this, SLOT(addBookmark()));
    connect(actionTextTranslator,SIGNAL(triggered()),this,SLOT(showLightTranslator()));
    connect(actionDetachTab,SIGNAL(triggered()),this,SLOT(detachTab()));
    connect(actionFindText,SIGNAL(triggered()),this,SLOT(findText()));
    connect(actionDictionary,SIGNAL(triggered()),gSet,SLOT(showDictionaryWindow()));
    connect(actionFullscreen,SIGNAL(triggered()),this,SLOT(switchFullscreen()));
    connect(tabMain, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
    connect(tabHelper, SIGNAL(tabLeftPostClicked(int)), this, SLOT(helperClicked(int)));
    connect(tabHelper, SIGNAL(tabLeftClicked(int)), this, SLOT(helperPreClicked(int)));
    connect(helperList, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            this, SLOT(helperItemClicked(QListWidgetItem*,QListWidgetItem*)));
    connect(splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(splitterMoved(int,int)));
    connect(tabMain, SIGNAL(tooltipRequested(QPoint,QPoint)), this, SLOT(tabBarTooltip(QPoint,QPoint)));

    if (gSet->logWindow!=NULL)
        connect(actionShowLog,SIGNAL(triggered()),gSet->logWindow,SLOT(show()));
    if (gSet->downloadManager!=NULL)
        connect(actionDownloadManager,SIGNAL(triggered()),gSet->downloadManager,SLOT(show()));

    QShortcut* sc;
    sc = new QShortcut(QKeySequence(Qt::CTRL+Qt::Key_Left),this);
    sc->setContext(Qt::WindowShortcut);
    connect(sc,SIGNAL(activated()),tabMain,SLOT(selectPrevTab()));
    sc = new QShortcut(QKeySequence(Qt::CTRL+Qt::Key_Right),this);
    sc->setContext(Qt::WindowShortcut);
    connect(sc,SIGNAL(activated()),tabMain,SLOT(selectNextTab()));

    centerWindow();

    reloadCharsetList();
    updateBookmarks();
    updateQueryHistory();
    updateRecycled();

    if (withSearch) createSearch();
    if (withViewer) createStartBrowser();

    qApp->installEventFilter(this);

    fullScreen=false;
    savedPos=pos();
    savedSize=size();
    savedMaximized=isMaximized();
    savedSplitterWidth=splitter->handleWidth();
}

CMainWindow::~CMainWindow()
{
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
    CSnippetViewer* pt = NULL;
    for (int i=0;i<tabMain->count();i++) {
        CSnippetViewer* sn = qobject_cast<CSnippetViewer *>(tabMain->widget(i));
        if (sn==NULL) continue;
        if (!sn->isStartPage) continue;
        pt = sn;
        break;
    }
    if (pt!=NULL)
        pt->closeTab(true);
}

void CMainWindow::tabBarTooltip(const QPoint &globalPos, const QPoint &)
{
    QPoint p = tabMain->tabBar()->mapFromGlobal(globalPos);
    int idx = tabMain->tabBar()->tabAt(p);
    if (idx<0) return;

    CSpecToolTipLabel* t = NULL;

    CSnippetViewer* sv = qobject_cast<CSnippetViewer *>(tabMain->widget(idx));
    if (sv!=NULL) {
        t = new CSpecToolTipLabel(sv->tabTitle);

        QPixmap pix = sv->txtBrowser->grab().scaledToWidth(250,Qt::SmoothTransformation);
        QPainter dc(&pix);
        QRect rx = pix.rect();
        QFont f = dc.font();
        f.setPointSize(8);
        dc.setFont(f);
        rx.setHeight(dc.fontMetrics().height());
        dc.fillRect(rx,t->palette().toolTipBase());
        dc.setPen(t->palette().toolTipText().color());
        dc.drawText(rx,Qt::AlignLeft | Qt::AlignVCenter,sv->tabTitle);

        t->setPixmap(pix);
    } else {
        CSearchTab* bt = qobject_cast<CSearchTab *>(tabMain->widget(idx));
        if (bt!=NULL) {
            t = new CSpecToolTipLabel(tr("<b>Search:</b> %1").arg(bt->getLastQuery()));
        }
    }

    if (t!=NULL)
        QxtToolTip::show(globalPos,t,tabMain->tabBar());
}

void CMainWindow::detachTab()
{
    if (tabMain->currentWidget()!=NULL) {
        CSpecTabContainer *bt = qobject_cast<CSpecTabContainer *>(tabMain->currentWidget());
        if (bt!=NULL)
            bt->detachTab();
    }
}

void CMainWindow::findText()
{
    if (tabMain->currentWidget()!=NULL) {
        CSnippetViewer* sn = qobject_cast<CSnippetViewer *>(tabMain->currentWidget());
        if (sn!=NULL)
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
    if (splitter->handle(0)!=NULL)
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
        CSpecTabContainer* sc = qobject_cast<CSpecTabContainer *>(tabMain->widget(i));
        if (sc!=NULL)
            sc->setToolbarVisibility(visible);
    }
}

void CMainWindow::splitterMoved(int, int)
{
    helperVisible=(helperFrame->width()>0);
}

void CMainWindow::centerWindow()
{
    int screen = 0;
    QWidget *w = window();
    QDesktopWidget *desktop = QApplication::desktop();
    if (w) {
        screen = desktop->screenNumber(w);
    } else if (desktop->isVirtualDesktop()) {
        screen = desktop->screenNumber(QCursor::pos());
    } else {
        screen = desktop->screenNumber(this);
    }
    QRect rect(desktop->availableGeometry(screen));
	int h = 80*rect.height()/100;
    QSize nw(135*h/100,h);
    if (nw.width()<1000) nw.setWidth(80*rect.width()/100);
    resize(nw);
    if (gSet->mainWindows.count()==0) {
        move(rect.width()/2 - frameGeometry().width()/2,
             rect.height()/2 - frameGeometry().height()/2);
    } else {
        CMainWindow* w = gSet->mainWindows.last();
        if (QApplication::activeWindow()!=NULL)
            if (qobject_cast<CMainWindow *>(QApplication::activeWindow())!=NULL)
                w = qobject_cast<CMainWindow *>(QApplication::activeWindow());
        move(w->pos().x()+25,w->pos().y()+25);
    }
}

void CMainWindow::updateQueryHistory()
{
    for(int i=0;i<tabMain->count();i++) {
        CSearchTab* t = qobject_cast<CSearchTab*>(tabMain->widget(i));
        if (t!=NULL) t->updateQueryHistory();
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
	lastTabIdx=savedTabIdx;
	savedTabIdx=idx;

    tabMain->tabBar()->setTabTextColor(idx,QApplication::palette(tabMain->tabBar()).windowText().color());
    if (tabMain->widget(idx)!=NULL) {
        CSnippetViewer *sn = qobject_cast<CSnippetViewer *>(tabMain->widget(idx));
        if (sn!=NULL) {
            sn->translationBkgdFinished=false;
            sn->loadingBkgdFinished=false;
            sn->txtBrowser->setFocus(Qt::OtherFocusReason);
        }
    }

    updateTitle();
    updateHelperList();

    if (tabMain->currentWidget()!=NULL) {
        CSnippetViewer* bt = qobject_cast<CSnippetViewer *>(tabMain->currentWidget());
        actionAddBM->setEnabled(bt!=NULL);
    }
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
                CSnippetViewer* sv = qobject_cast<CSnippetViewer*>(tabMain->widget(i));
                if (sv!=NULL) {
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
                CSearchTab* bv = qobject_cast<CSearchTab*>(tabMain->widget(i));
                if (bv!=NULL) {
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
                QListWidgetItem* it = new QListWidgetItem(gSet->recycleBin[i].title);
                it->setData(Qt::UserRole,1);
                it->setData(Qt::UserRole+1,i);
                helperList->addItem(it);
            }
            break;
        case 2: // History
            foreach (const UrlHolder &t, gSet->mainHistory) {
                QListWidgetItem* it = new QListWidgetItem(t.title);
                it->setStatusTip(t.url.toString());
                it->setToolTip(t.url.toString());
                it->setData(Qt::UserRole,2);
                it->setData(Qt::UserRole+1,0);
                it->setData(Qt::UserRole+2,t.uuid.toString());
                helperList->addItem(it);
            }
            break;
    }
}

void CMainWindow::helperItemClicked(QListWidgetItem *current, QListWidgetItem *)
{
    if (current==NULL) return;
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
            u = gSet->recycleBin.at(idx).url;
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
    foreach (const UrlHolder &t, gSet->mainHistory) {
        QListWidgetItem* it = new QListWidgetItem(t.title);
        it->setStatusTip(t.url.toString());
        it->setToolTip(t.url.toString());
        it->setData(Qt::UserRole,2);
        it->setData(Qt::UserRole+1,0);
        it->setData(Qt::UserRole+2,t.uuid.toString());
        helperList->addItem(it);
    }
}

void CMainWindow::updateTitle()
{
    QString t = tr("JPReader");
    if (tabMain->currentWidget()!=NULL) {
        CSnippetViewer* sv = qobject_cast<CSnippetViewer*>(tabMain->currentWidget());
        if (sv!=NULL && !sv->tabTitle.isEmpty()) {
            QTextDocument doc;
            doc.setHtml(sv->tabTitle);
            t = doc.toPlainText() + " - " + t;
            t.remove("\r");
            t.remove("\n");
        }
        CSearchTab* bv = qobject_cast<CSearchTab*>(tabMain->currentWidget());
        if (bv!=NULL && !bv->getLastQuery().isEmpty()) t =
                tr("[%1] search - %2").arg(bv->getLastQuery()).arg(t);
    }
    setWindowTitle(t);
}

void CMainWindow::goHistory(QUuid idx)
{
    QListIterator<UrlHolder> li(gSet->mainHistory);
    UrlHolder i;
    while (li.hasNext()) {
        i=li.next();
        if (i.uuid==idx) break;
    }
    if (i.uuid!=idx) return;
    QUrl u = i.url;
    if (!u.isValid()) return;
    new CSnippetViewer(this, u);
}

void CMainWindow::createSearch()
{
    new CSearchTab(this);
}

void CMainWindow::createStartBrowser()
{
    QFile f(":/data/startpage");
    QString html;
    if (f.open(QIODevice::ReadOnly))
        html = QString::fromUtf8(f.readAll());
    new CSnippetViewer(this,QUrl(),QStringList(),true,html,"100%",true);
}

void CMainWindow::checkTabs()
{
    if(tabMain->count()==0) close();
}

void CMainWindow::openAuxFile()
{
    QStringList fnames = getOpenFileNamesD(this,tr("Open text file"),gSet->savedAuxDir);
    if (fnames.isEmpty()) return;

    gSet->savedAuxDir = QFileInfo(fnames.first()).absolutePath();

    for(int i=0;i<fnames.count();i++) {
        if (fnames.at(i).isEmpty()) continue;
        new CSnippetViewer(this, QUrl::fromLocalFile(fnames.at(i)));
    }
}

void CMainWindow::openEmptyBrowser()
{
    CSnippetViewer* sv = new CSnippetViewer(this);
    sv->urlEdit->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::openAuxFileInDir()
{
    QWidget* t = tabMain->currentWidget();
    CSnippetViewer* sv = qobject_cast<CSnippetViewer *>(t);
    if (sv==NULL) {
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

    gSet->savedAuxDir = QDir(fnames.first()).absolutePath();

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
    url.setFragment("");
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
    //menuBookmarks->clear();
    while (menuBookmarks->actions().count()>2)
        menuBookmarks->removeAction(menuBookmarks->actions().last());
    foreach (const QString &t, gSet->bookmarks.keys()) {
		QAction* a = menuBookmarks->addAction(t,this,SLOT(openBookmark()));
        a->setData(gSet->bookmarks.value(t));
        a->setStatusTip(gSet->bookmarks.value(t).toString());
	}
}

void CMainWindow::addBookmark()
{
    if (tabMain->currentWidget()==NULL) return;
    CSnippetViewer* sv = qobject_cast<CSnippetViewer *>(tabMain->currentWidget());
    if (sv==NULL) return;

    CBookmarkDlg *dlg = new CBookmarkDlg(sv,sv->tabTitle,sv->getUrl().toString());
    if (dlg->exec()) {
        QString t = dlg->getBkTitle();
        if (!t.isEmpty() && !gSet->bookmarks.contains(t)) {
            gSet->bookmarks[t]=QUrl::fromUserInput(dlg->getBkUrl());
            gSet->updateAllBookmarks();
        } else
            QMessageBox::warning(this,tr("JPReader"),
                                 tr("Unable to add bookmark (frame is empty or duplicate title). Try again."));
    }
    dlg->setParent(NULL);
    delete dlg;
}

void CMainWindow::showLightTranslator()
{
    if (gSet->lightTranslator==NULL)
        gSet->lightTranslator = new CLightTranslator();

    gSet->lightTranslator->restoreWindow();
}

void CMainWindow::openBookmark()
{
	QAction* a = qobject_cast<QAction *>(sender());
	if (a==NULL) return;

	QUrl u = a->data().toUrl();
	if (!u.isValid()) return;
    new CSnippetViewer(this, u);
}

void CMainWindow::openRecycled()
{
    QAction* a = qobject_cast<QAction *>(sender());
    if (a==NULL) return;

    bool okconv;
    int idx = a->data().toInt(&okconv);
    if (!okconv) return;
    if (idx<0 || idx>=gSet->recycleBin.count()) return;
    QUrl u = gSet->recycleBin.at(idx).url;
    if (!u.isValid()) return;
    new CSnippetViewer(this, u);

    gSet->recycleBin.removeAt(idx);
    updateRecycled();
}

void CMainWindow::updateRecycled()
{
    recycledMenu->clear();
    for (int i=0;i<gSet->recycleBin.count();i++) {
        QAction* a = recycledMenu->addAction(gSet->recycleBin[i].title,this,SLOT(openRecycled()));
        a->setStatusTip(gSet->recycleBin[i].url.toString());
        a->setData(i);
    }
    updateHelperList();
}

void CMainWindow::updateTabs()
{
    tabsMenu->clear();
    for(int i=0;i<tabMain->count();i++) {
        CSpecTabContainer* sv = qobject_cast<CSpecTabContainer*>(tabMain->widget(i));
		if (sv==NULL) continue;
        tabsMenu->addAction(sv->getDocTitle(),this,SLOT(activateTab()))->setData(i);
    }
    updateHelperList();
}

void CMainWindow::activateTab()
{
    QAction* act = qobject_cast<QAction*>(sender());
    if(act==NULL) return;
    bool okconv;
    int idx = act->data().toInt(&okconv);
    if (!okconv) return;
    tabMain->setCurrentIndex(idx);
}

void CMainWindow::forceCharset()
{
    QAction* act = qobject_cast<QAction*>(sender());
    if (act==NULL) return;

    QString cs = act->data().toString();
    if (!cs.isEmpty()) {
        if (QTextCodec::codecForName(cs.toLatin1().data())!=NULL)
            cs = QTextCodec::codecForName(cs.toLatin1().data())->name();

        gSet->charsetHistory.removeAll(cs);
        gSet->charsetHistory.prepend(cs);
        if (gSet->charsetHistory.count()>10)
            gSet->charsetHistory.removeLast();
    }
    gSet->forcedCharset = cs;
    gSet->updateAllCharsetLists();

    if (gSet->webProfile!=NULL &&
            gSet->webProfile->settings()!=NULL)
        gSet->webProfile->settings()->setDefaultTextEncoding(cs);
}

void CMainWindow::reloadCharsetList()
{
    QAction* act;
    menuCharset->clear();
    act = menuCharset->addAction(tr("Autodetect"),this,SLOT(forceCharset()));
    act->setData(QString(""));
    if (gSet->forcedCharset.isEmpty()) {
        act->setCheckable(true);
        act->setChecked(true);
    } else {
        act = menuCharset->addAction(gSet->forcedCharset,this,SLOT(forceCharset()));
        act->setData(gSet->forcedCharset);
        act->setCheckable(true);
        act->setChecked(true);
    }
    menuCharset->addSeparator();

    QMenu* midx = menuCharset;
    QList<QStringList> cList = encodingsByScript();
    for(int i=0;i<cList.count();i++) {
        midx = menuCharset->addMenu(cList.at(i).at(0));
        for(int j=1;j<cList.at(i).count();j++) {
            if (QTextCodec::codecForName(cList.at(i).at(j).toLatin1())==NULL) {
                qWarning() << tr("Encoding %1 not supported.").arg(cList.at(i).at(j));
                continue;
            }
            QString cname = QTextCodec::codecForName(cList.at(i).at(j).toLatin1())->name();
            act = midx->addAction(cname,this,SLOT(forceCharset()));
            act->setData(cname);
            if (QTextCodec::codecForName(cname.toLatin1().data())!=NULL) {
                if (QTextCodec::codecForName(cname.toLatin1().data())->name()==gSet->forcedCharset) {
                    act->setCheckable(true);
                    act->setChecked(true);
                }
            }
        }
    }
    menuCharset->addSeparator();

    for(int i=0;i<gSet->charsetHistory.count();i++) {
        if (gSet->charsetHistory.at(i)==gSet->forcedCharset) continue;
        act = menuCharset->addAction(gSet->charsetHistory.at(i),this,SLOT(forceCharset()));
        act->setData(gSet->charsetHistory.at(i));
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
    if (fullScreen && cln.contains("WebEngine",Qt::CaseInsensitive)) {
        if (ev->type()==QEvent::MouseMove) {
            QMouseEvent* mev = static_cast<QMouseEvent *>(ev);
            if (mev!=NULL) {
                if (mev->y()<20 && !tabMain->tabBar()->isVisible())
                    setToolsVisibility(true);
                else if (mev->y()>25 && tabMain->tabBar()->isVisible())
                    setToolsVisibility(false);
            }
        }
    }
    return QMainWindow::eventFilter(obj,ev);
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
                     "Baloo backend compiled: %6\n\n"
                     "Poppler support: %7\n\n"
                     "WebEngine version.")
                    .arg(BUILD_REV)
                    .arg(debugstr)
                    .arg(BUILD_PLATFORM)
                    .arg(BUILD_DATE)
                    .arg(recoll)
                    .arg(baloo5)
                    .arg(poppler);

    QMessageBox::about(this, tr("JPReader"),msg);
}
