#include <QShortcut>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QTextCodec>
#include <QAbstractNetworkCache>
#include <QTextDocument>

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

CMainWindow::CMainWindow(bool withSearch, bool withViewer)
        : MainWindow()
{
	setupUi(this);

    tabMain->parentWnd=this;
	lastTabIdx=0;
	savedTabIdx=0;
    savedHelperWidth=0;
    setWindowIcon(gSet->appIcon);

    stSearchStatus.setText(tr("Ready"));
    stSearchStatus.setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    statusBar()->addPermanentWidget(&stSearchStatus);

    helperVisible = false;
    tabHelper = new QSpecTabBar(this);
    tabHelper->setShape(QTabBar::RoundedWest);

    //splitter->insertWidget(0,tabHelper);
    QHBoxLayout *hbx = qobject_cast<QHBoxLayout *>(centralWidget()->layout());
    if (hbx!=NULL)
        hbx->insertWidget(0,tabHelper,0,Qt::AlignTop);
    else
        qDebug() << "Main HBox layout not found. Check form design, update source.";
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
    connect(actionClearCaches,SIGNAL(triggered()),gSet,SLOT(clearCaches()));
    connect(actionSaveSettings,SIGNAL(triggered()), gSet, SLOT(writeSettings()));
    connect(actionAddBM,SIGNAL(triggered()),this, SLOT(addBookmark()));
    connect(actionTextTranslator,SIGNAL(triggered()),this,SLOT(showLightTranslator()));
    connect(actionDetachTab,SIGNAL(triggered()),this,SLOT(detachTab()));
    connect(actionFindText,SIGNAL(triggered()),this,SLOT(findText()));
    connect(actionDictionary,SIGNAL(triggered()),gSet,SLOT(showDictionaryWindow()));
    connect(tabMain, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
    connect(tabHelper, SIGNAL(tabLeftPostClicked(int)), this, SLOT(helperClicked(int)));
    connect(tabHelper, SIGNAL(tabLeftClicked(int)), this, SLOT(helperPreClicked(int)));
    connect(helperList, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            this, SLOT(helperItemClicked(QListWidgetItem*,QListWidgetItem*)));
    connect(splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(splitterMoved(int,int)));
    connect(tabMain, SIGNAL(tooltipRequested(QPoint,QPoint)), this, SLOT(tabBarTooltip(QPoint,QPoint)));

    if (gSet->logWindow!=NULL)
        connect(actionShowLog,SIGNAL(triggered()),gSet->logWindow,SLOT(show()));

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

void CMainWindow::hideTooltip()
{
    QxtToolTip::setToolTip(tabMain->tabBar(),NULL);
}

void CMainWindow::tabBarTooltip(const QPoint &globalPos, const QPoint &)
{
    QPoint p = tabMain->tabBar()->mapFromGlobal(globalPos);
    int idx = tabMain->tabBar()->tabAt(p);
    if (idx<0) return;

    QSpecToolTipLabel* t = NULL;

    CSnippetViewer* sv = qobject_cast<CSnippetViewer *>(tabMain->widget(idx));
    if (sv!=NULL) {
        t = new QSpecToolTipLabel(sv->tabTitle);
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
        t->setPixmap(QPixmap::grabWidget(sv->txtBrowser).scaledToWidth(250,Qt::SmoothTransformation));
#else
        t->setPixmap(sv->txtBrowser->grab().scaledToWidth(250,Qt::SmoothTransformation));
#endif
    } else {
        CSearchTab* bt = qobject_cast<CSearchTab *>(tabMain->widget(idx));
        if (bt!=NULL) {
            t = new QSpecToolTipLabel(tr("<b>Search:</b> %1").arg(bt->getLastQuery()));
        }
    }

    if (t!=NULL) {
        connect(t,SIGNAL(labelHide()),this,SLOT(hideTooltip()));
        QxtToolTip::show(globalPos,t,tabMain->tabBar());
    }
}

void CMainWindow::detachTab()
{
    if (tabMain->currentWidget()!=NULL) {
        QSpecTabContainer *bt = qobject_cast<QSpecTabContainer *>(tabMain->currentWidget());
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
                    it->setStatusTip(sv->Uri.toString());
                    it->setToolTip(sv->Uri.toString());
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
    updateSuggestionLists(NULL);
}

void CMainWindow::updateSuggestionLists(CSnippetViewer* snviewer)
{
    QStringList sg;
    for (int i=0;i<200;i++) {
        if (i>=gSet->mainHistory.count()) break;
        sg << gSet->mainHistory.at(i).url.toString();
    }
    if (snviewer==NULL) {
        for (int i=0;i<tabMain->count();i++) {
            CSnippetViewer* sn = qobject_cast<CSnippetViewer *>(tabMain->widget(i));
            if (sn!=NULL)
                sn->updateHistorySuggestion(sg);
        }
    } else
        snviewer->updateHistorySuggestion(sg);
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
    QStringList fnames = getOpenFileNamesD(this,tr("Open HTML file"),gSet->savedAuxDir);
    if (fnames.isEmpty()) return;

    gSet->savedAuxDir = QDir(fnames.first()).absolutePath();

    for(int i=0;i<fnames.count();i++) {
        if (fnames.at(i).isNull() || fnames.at(i).isEmpty()) continue;
        QString su = "file://" + fnames.at(i);
        new CSnippetViewer(this, su);
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
        QMessageBox::warning(this,tr("JPReader error"),tr("Active document viewer tab not found."));
        return;
    }
    QString auxDir = sv->Uri.toLocalFile();
    if (auxDir.isEmpty()) {
        QMessageBox::warning(this,tr("JPReader error"),tr("Remote document opened. Cannot define local directory."));
        return;
    }
    QStringList fnames = getOpenFileNamesD(this,tr("Open HTML file"),auxDir);
    if (fnames.isEmpty()) return;

    gSet->savedAuxDir = QDir(fnames.first()).absolutePath();

    for(int i=0;i<fnames.count();i++) {
        if (fnames.at(i).isNull() || fnames.at(i).isEmpty()) continue;
        QString su = "file://" + fnames.at(i);
        new CSnippetViewer(this, su);
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
    sv->urlEdit->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::createFromClipboardPlain()
{
    QString tx = getClipboardContent(true,true);
    if (tx.isEmpty()) {
        QMessageBox::information(this, tr("JPReader"),tr("Clipboard is empty or contains incompatible data."));
        return;
    }
    CSnippetViewer* sv = new CSnippetViewer(this, QUrl(), QStringList(), true, tx);
    sv->urlEdit->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::clearClipboard()
{
    QApplication::clipboard()->clear(QClipboard::Clipboard);
}

void CMainWindow::openFromClipboard()
{
    QUrl url = QUrl::fromUserInput(getClipboardContent(true));
    qDebug() << url;
    url.setFragment("");
    QString uri = url.toString().remove(QRegExp("#$"));
    if (uri.isEmpty()) {
        QMessageBox::information(this, tr("JPReader"),tr("Clipboard is empty or contains incompatible data."));
        return;
    }
    CSnippetViewer* sv = new CSnippetViewer(this, uri);
    sv->urlEdit->setFocus(Qt::OtherFocusReason);
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

    CBookmarkDlg *dlg = new CBookmarkDlg(sv,sv->tabTitle,sv->Uri.toString());
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
        QSpecTabContainer* sv = qobject_cast<QSpecTabContainer*>(tabMain->widget(i));
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
    if (QTextCodec::codecForName(cs.toLatin1().data())!=NULL)
        cs = QTextCodec::codecForName(cs.toLatin1().data())->name();
    gSet->forcedCharset = cs;
    gSet->updateAllCharsetLists();
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
            act = midx->addAction(cList.at(i).at(j),this,SLOT(forceCharset()));
            act->setData(cList.at(i).at(j));
            QString cname = cList.at(i).at(j);
            if (QTextCodec::codecForName(cname.toLatin1().data())!=NULL) {
                if (QTextCodec::codecForName(cname.toLatin1().data())->name()==gSet->forcedCharset) {
                    act->setCheckable(true);
                    act->setChecked(true);
                }
            }
        }
    }
}

void CMainWindow::closeEvent(QCloseEvent *event)
{
    emit aboutToClose(this);
    event->accept();
}

void CMainWindow::helpAbout()
{
    QString nepomuk = tr("no");
    QString recoll = tr("no");
    QString baloo5 = tr("no");
    QString debugstr;
    debugstr.clear();
#ifdef QT_DEBUG
    debugstr = tr ("Debug build.\n");
#endif
#ifdef WITH_NEPOMUK
    nepomuk = tr("yes");
#endif
#ifdef WITH_RECOLL
    recoll = tr("yes");
#endif
#ifdef WITH_BALOO5
    baloo5 = tr("yes");
#endif
    QString msg = tr("JPReader for searching, translating and reading text files in Japanese\n\n"
                     "Build: %1 %2\n"
                     "Platform: %3\n"
                     "Build date: %4\n\n"
                     "Nepomuk backend compiled: %5\n"
                     "Recoll backend compiled: %6\n"
                     "Baloo (KDE Frameworks 5) backend compiled: %7")
                    .arg(BUILD_REV)
                    .arg(debugstr)
                    .arg(BUILD_PLATFORM)
                    .arg(BUILD_DATE)
                    .arg(nepomuk)
                    .arg(recoll)
                    .arg(baloo5);

    QMessageBox::about(this, tr("JPReader"),msg);
}
