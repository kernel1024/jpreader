#include <QShortcut>
#include <QScreen>
#include <QWindow>
#include <QMessageBox>
#include <QAbstractNetworkCache>
#include <QTextDocument>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QMimeData>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>

#include "miniqxt/qxttooltip.h"
#include "mainwindow.h"
#include "global/control.h"
#include "global/settings.h"
#include "global/startup.h"
#include "global/ui.h"
#include "global/network.h"
#include "global/history.h"
#include "global/actions.h"
#include "browser/browser.h"
#include "browser/ctxhandler.h"
#include "browser-utils/downloadmanager.h"
#include "browser-utils/bookmarks.h"
#include "browser-utils/downloadmanager.h"
#include "browser-utils/autofillassistant.h"
#include "search/searchtab.h"
#include "utils/settingstab.h"
#include "utils/genericfuncs.h"
#include "utils/specwidgets.h"
#include "utils/logdisplay.h"
#include "utils/workermonitor.h"
#include "utils/settingstab.h"
#include "utils/pixivindextab.h"
#include "translator/lighttranslator.h"
#include "translator/translatorstatisticstab.h"
#include "translator/translatorcache.h"
#include "extractors/abstractextractor.h"
#include "extractors/pixivindexextractor.h"
#include "manga/mangaviewtab.h"

namespace CDefaults {
const int titleRenameLockTimeout = 500;
const int statusBarMessageTimeout = 5000;
}

CMainWindow::CMainWindow(bool withSearch, bool withViewer, const QVector<QUrl> &viewerUrls, QWidget *parent)
    : QMainWindow(parent)
{
    setupUi(this);

    tabMain->setParentWnd(this);
    
    setWindowIcon(gSet->ui()->appIcon());
    setAttribute(Qt::WA_DeleteOnClose,true);

    tabMain->tabBar()->setBrowserTabs(true);
    
    stSearchStatus = new QLabel(this);
    stSearchStatus->setText(tr("Ready"));
    stSearchStatus->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    statusBar()->addPermanentWidget(stSearchStatus);
    
    tabHelper = new CSpecTabBar(this);
    tabHelper->setShape(QTabBar::RoundedWest);

    auto *hbx = qobject_cast<QHBoxLayout *>(centralWidget()->layout());
    if (hbx == nullptr)
        qFatal("Main HBox layout not found. Check form design, update source.");
    hbx->insertWidget(0,tabHelper,0,Qt::AlignTop);

    tabHelper->addTab(tr("Tabs"));
    tabHelper->addTab(tr("Recycled"));
    tabHelper->addTab(tr("History"));
    updateSplitters();

    recentMenu = new QMenu(menuFile);
    actionRecentDocuments->setMenu(recentMenu);

    menuBar()->addSeparator();
    tabsMenu = menuBar()->addMenu(QIcon::fromTheme(QSL("tab-detach")),QString());
    recycledMenu = menuBar()->addMenu(QIcon::fromTheme(QSL("user-trash")),QString());

    menuBookmarks->setStyleSheet(QSL("QMenu { menu-scrollable: 1; }"));
    menuBookmarks->setToolTipsVisible(true);

    titleRenamedLock.setInterval(CDefaults::titleRenameLockTimeout);
    titleRenamedLock.setSingleShot(true);

    connect(actionAbout, &QAction::triggered, this, &CMainWindow::helpAbout);
    connect(actionAboutQt, &QAction::triggered, gSet->app(), &QApplication::aboutQt);
    connect(actionSettings, &QAction::triggered, gSet->ui(), &CGlobalUI::settingsTab);
    connect(actionExit, &QAction::triggered, this, &CMainWindow::close);
    connect(actionExitAll, &QAction::triggered, gSet->startup(), &CGlobalStartup::cleanupAndExit);
    connect(actionOpen, &QAction::triggered, this, &CMainWindow::openAuxFileWithDialog);
    connect(actionNew, &QAction::triggered, this, &CMainWindow::openEmptyBrowser);
    connect(actionOpenInDir, &QAction::triggered, this, &CMainWindow::openAuxFileInDir);
    connect(actionOpenCB, &QAction::triggered, this, &CMainWindow::createFromClipboard);
    connect(actionOpenCBPlain, &QAction::triggered, this, &CMainWindow::createFromClipboardPlain);
    connect(actionOpenPixivList, &QAction::triggered, this, &CMainWindow::openPixivList);
    connect(actionSave, &QAction::triggered, this, &CMainWindow::save);
    connect(actionClearCB, &QAction::triggered, this, &CMainWindow::clearClipboard);
    connect(actionClearCache, &QAction::triggered, gSet->net(), &CGlobalNetwork::clearCaches);
    connect(actionOpenClip, &QAction::triggered, this, &CMainWindow::openFromClipboard);
    connect(actionWnd, &QAction::triggered, gSet->ui(), &CGlobalUI::addMainWindow);
    connect(actionNewSearch, &QAction::triggered, this, &CMainWindow::createSearch);
    connect(actionAddBM, &QAction::triggered, this, &CMainWindow::addBookmark);
    connect(actionManageBM, &QAction::triggered, this, &CMainWindow::manageBookmarks);
    connect(actionTextTranslator, &QAction::triggered, this, &CMainWindow::showLightTranslator);
    connect(actionDetachTab, &QAction::triggered, this, &CMainWindow::detachTab);
    connect(actionFindText, &QAction::triggered, this, &CMainWindow::findText);
    connect(actionDictionary, &QAction::triggered, gSet->ui(), &CGlobalUI::showDictionaryWindow);
    connect(actionFullscreen, &QAction::triggered, this, &CMainWindow::switchFullscreen);
    connect(actionChromiumURLs, &QAction::triggered, this, &CMainWindow::openChromiumURLs);
    connect(actionTranslatorStatistics, &QAction::triggered, gSet->ui(), &CGlobalUI::translationStatisticsTab);
    connect(actionTranslatorCache, &QAction::triggered, gSet->translatorCache(), &CTranslatorCache::showDialog);
    connect(actionPixivSearch, &QAction::triggered, this, &CMainWindow::pixivSearch);
    connect(actionPrintPDF, &QAction::triggered, this, &CMainWindow::printToPDF);
    connect(actionSaveSettings,&QAction::triggered, gSet, &CGlobalControl::writeSettings);
    connect(tabMain, &CSpecTabWidget::currentChanged, this, &CMainWindow::tabChanged);
    connect(tabHelper, &CSpecTabBar::tabLeftPostClicked, this, &CMainWindow::helperClicked);
    connect(tabHelper, &CSpecTabBar::tabLeftClicked, this, &CMainWindow::helperPreClicked);
    connect(helperList, &QListWidget::currentItemChanged, this, &CMainWindow::helperItemClicked);
    connect(splitter, &QSplitter::splitterMoved, this, &CMainWindow::splitterMoved);
    connect(tabMain, &CSpecTabWidget::tooltipRequested, this, &CMainWindow::tabBarTooltip);

    connect(gSet->dictionaryManager(), &ZDict::ZDictController::dictionariesLoaded,
            this, &CMainWindow::displayStatusBarMessage,Qt::QueuedConnection);

    if (gSet->logWindow()!=nullptr)
        connect(actionShowLog, &QAction::triggered, gSet->logWindow(), &CLogDisplay::show);

    if (gSet->downloadManager()!=nullptr) {
        connect(actionDownloadManager, &QAction::triggered,
                gSet->downloadManager(), &CDownloadManager::show);
    }

    if (gSet->workerMonitor()!=nullptr) {
        connect(actionWorkerMonitor, &QAction::triggered,
                gSet->workerMonitor(), &CWorkerMonitor::show);
    }

    if (gSet->autofillAssistant()!=nullptr) {
        connect(actionAutofillAssistant, &QAction::triggered,
                gSet->autofillAssistant(), &CAutofillAssistant::show);
    }

    QShortcut* sc = nullptr;
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
                new CBrowserTab(this,url);
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

    QApplication::instance()->installEventFilter(this);

    auto *addTabButton = new QPushButton(QIcon::fromTheme(QSL("list-add")),QString());
    addTabButton->setFlat(true);
    tabMain->setCornerWidget(addTabButton);
    connect(addTabButton,&QPushButton::clicked,actionNew,&QAction::trigger);

    setAcceptDrops(true);
}

void CMainWindow::updateSplitters()
{
    const int minHelperWidth = 150;

    splitter->setCollapsible(0,true);
    if (savedHelperWidth<minHelperWidth) savedHelperWidth=minHelperWidth;

    QList<int> widths;
    if (helperVisible) {
        widths << savedHelperWidth;
    } else {
        widths << 0;
    }
    if (!helperVisible && helperFrame->width()>0)
        savedHelperWidth=helperFrame->width();

    widths << splitter->width()-widths[0];
    splitter->setSizes(widths);
}

void CMainWindow::closeStartPage()
{
    CBrowserTab* pt = nullptr;
    for (int i=0;i<tabMain->count();i++) {
        auto *sn = qobject_cast<CBrowserTab *>(tabMain->widget(i));
        if (sn==nullptr) continue;
        if (!sn->isStartPage()) continue;
        pt = sn;
        break;
    }
    if (pt)
        pt->closeTab(true);
}

void CMainWindow::tabBarTooltip(const QPoint &globalPos, const QPoint &localPos)
{
    Q_UNUSED(localPos)

    const QSize tooltipPreviewSize(250,100);
    const int tooltipFontSize = 8;

    QPoint p = tabMain->tabBar()->mapFromGlobal(globalPos);
    int idx = tabMain->tabBar()->tabAt(p);
    if (idx<0) return;

    auto *sv = qobject_cast<CBrowserTab *>(tabMain->widget(idx));
    if (sv) {
        QPixmap pix = sv->pageImage();
        bool fillRect = false;
        if (pix.isNull()) {
            pix = QPixmap(tooltipPreviewSize);
            fillRect = true;
        } else
            pix = pix.scaledToWidth(tooltipPreviewSize.width(),Qt::SmoothTransformation);
        QPainter dc(&pix);
        QRect rx = pix.rect();
        QFont f = dc.font();
        f.setPointSize(tooltipFontSize);
        dc.setFont(f);
        if (fillRect)
            dc.fillRect(rx,palette().window());
        rx.setHeight(dc.fontMetrics().height());
        dc.fillRect(rx,palette().toolTipBase());
        dc.setPen(palette().toolTipText().color());
        dc.drawText(rx,Qt::AlignLeft | Qt::AlignVCenter,sv->tabTitle());

        auto *t = new QLabel();
        t->setPixmap(pix);
        QxtToolTip::show(globalPos,t,tabMain->tabBar());
        return;
    }

    auto *bt = qobject_cast<CSearchTab *>(tabMain->widget(idx));
    if (bt) {
        auto *t = new QLabel(tr("<b>Search:</b> %1").arg(bt->getLastQuery()));
        QxtToolTip::show(globalPos,t,tabMain->tabBar());
        return;
    }

    auto *ts = qobject_cast<CTranslatorStatisticsTab *>(tabMain->widget(idx));
    if (ts) {
        auto *t = new QLabel(tr("Translator statistics tab - %1").arg(ts->getTimeRangeString()));
        QxtToolTip::show(globalPos,t,tabMain->tabBar());
        return;
    }

    auto *st = qobject_cast<CSpecTabContainer *>(tabMain->widget(idx));
    if (st) {
        auto *t = new QLabel(st->tabTitle());
        QxtToolTip::show(globalPos,t,tabMain->tabBar());
        return;
    }
}

void CMainWindow::detachTab()
{
    if (tabMain->currentWidget()) {
        auto *bt = qobject_cast<CSpecTabContainer *>(tabMain->currentWidget());
        if (bt)
            bt->detachTab();
    }
}

void CMainWindow::findText()
{
    if (tabMain->currentWidget()) {
        auto *sn = qobject_cast<CBrowserTab *>(tabMain->currentWidget());
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

    if (fullScreen) {
        showFullScreen();
    } else {
        if (savedMaximized) {
            showMaximized();
        } else {
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
        auto *sc = qobject_cast<CSpecTabContainer *>(tabMain->widget(i));
        if (sc)
            sc->setToolbarVisibility(visible);
    }
}

void CMainWindow::printToPDF()
{
    if (tabMain->currentWidget()==nullptr) return;
    auto *sn = qobject_cast<CBrowserTab *>(tabMain->currentWidget());
    if (sn==nullptr) return;

    sn->printToPDF();
}

void CMainWindow::save()
{
    if (tabMain->currentWidget()==nullptr) return;

    auto *sv = qobject_cast<CBrowserTab *>(tabMain->currentWidget());
    if (sv)
        sv->save();

    auto *ts = qobject_cast<CTranslatorStatisticsTab *>(tabMain->currentWidget());
    if (ts)
        ts->save();

    auto *mv = qobject_cast<CMangaViewTab *>(tabMain->currentWidget());
    if (mv)
        mv->save();
}

void CMainWindow::displayStatusBarMessage(const QString &text)
{
    statusBar()->showMessage(text,CDefaults::statusBarMessageTimeout);
}

void CMainWindow::splitterMoved(int pos, int index)
{
    Q_UNUSED(pos)
    Q_UNUSED(index)

    helperVisible=(helperFrame->width()>0);
}

void CMainWindow::centerWindow()
{
    const int initialHeightFrac = 80;
    const int initialWidthFrac = 135;
    const int maxWidth = 1000;
    const int maxWidthFrac = 80;
    const int newWindowLadderMargin = 25;

    QScreen *screen = nullptr;

    if (window() && window()->windowHandle()) {
        screen = window()->windowHandle()->screen();
    } else if (!QApplication::screens().isEmpty()) {
        screen = QApplication::screenAt(QCursor::pos());
    }
    if (screen == nullptr)
        screen = QApplication::primaryScreen();

    QRect rect(screen->availableGeometry());
    int h = initialHeightFrac*rect.height()/100;
    QSize nw(initialWidthFrac*h/100,h);
    if (nw.width()<maxWidth) nw.setWidth(maxWidthFrac*rect.width()/100);
    resize(nw);
    QRect mwGeom = gSet->ui()->getLastMainWindowGeometry();
    if (mwGeom.isNull()) {
        move(rect.width()/2 - frameGeometry().width()/2,
             rect.height()/2 - frameGeometry().height()/2);
    } else {
        if (QApplication::activeWindow()!=nullptr)
            mwGeom = QApplication::activeWindow()->geometry();

        move(mwGeom.topLeft().x()+newWindowLadderMargin,
             mwGeom.topLeft().y()+newWindowLadderMargin);
    }
}

void CMainWindow::updateQueryHistory()
{
    for(int i=0;i<tabMain->count();i++) {
        auto *t = qobject_cast<CSearchTab*>(tabMain->widget(i));
        if (t)
            t->updateQueryHistory();
    }
}

void CMainWindow::helperPreClicked(int index)
{
    Q_UNUSED(index)

    savedHelperIdx = tabHelper->currentIndex();
}

void CMainWindow::helperClicked(int index)
{
    if (index<0 || index>=tabHelper->count()) return;
    if (helperVisible && savedHelperIdx==tabHelper->currentIndex()) {
        helperVisible=false;
    } else if (!helperVisible) {
        helperVisible=true;
    }
    updateSplitters();
    updateHelperList();
}

void CMainWindow::tabChanged(int idx)
{
    lastTabIdx=idx;

    if (tabMain->widget(idx)!=nullptr) {
        auto *sn = qobject_cast<CBrowserTab *>(tabMain->widget(idx));
        actionAddBM->setEnabled(sn != nullptr);
        auto *tab = qobject_cast<CSpecTabContainer *>(tabMain->widget(idx));
        if (tab)
            tab->tabAcquiresFocus();
    }

    updateTitle();
    updateHelperList();
}

void CMainWindow::setSearchStatus(const QString &text)
{
    stSearchStatus->setText(text);
}

void CMainWindow::startTitleRenameTimer()
{
    titleRenamedLock.start();
}

bool CMainWindow::isTitleRenameTimerActive() const
{
    return titleRenamedLock.isActive();
}

int CMainWindow::lastTabIndex() const
{
    return lastTabIdx;
}

bool CMainWindow::sendInputToActiveBrowser(const QString &text)
{
    QWidget *wv = tabMain->currentWidget();
    if (wv == nullptr) return false;

    auto *sv = qobject_cast<CBrowserTab*>(tabMain->currentWidget());
    if (sv == nullptr) return false;

    sv->sendInputToBrowser(text);
    return true;
}

void CMainWindow::updateHelperList()
{
    helperList->clear();
    switch (tabHelper->currentIndex()) {
        case 0: // Tabs
            for (int i=0;i<tabMain->count();i++) {
                auto *it = new QListWidgetItem(tr("Tab %1").arg(i));
                it->setData(Qt::UserRole,0);
                it->setData(Qt::UserRole+1,i);
                auto *sv = qobject_cast<CBrowserTab*>(tabMain->widget(i));
                if (sv) {
                    it->setText(sv->tabTitle());
                    QString url = CGenericFuncs::elideString(sv->getUrl().toString(),CDefaults::maxTitleElideLength);
                    it->setStatusTip(url);
                    it->setToolTip(url);

                    if (sv->isTranslationBkgdFinished()) {
                        it->setForeground(QBrush(Qt::green));

                    } else if (sv->isLoadingBkgdFinished()) {
                        it->setForeground(QBrush(Qt::blue));

                    } else {
                        it->setForeground(QApplication::palette(helperList).text());
                    }
                }

                auto *bv = qobject_cast<CSearchTab*>(tabMain->widget(i));
                if (bv) {
                    QString qr = bv->getLastQuery();
                    if (qr.isEmpty()) qr = tr("(empty)");
                    it->setText(tr("Search: %1").arg(qr));
                    it->setToolTip(qr);
                }

                auto *st = qobject_cast<CSpecTabContainer*>(tabMain->widget(i));
                if (st) {
                    it->setText(st->tabTitle());
                }
                helperList->addItem(it);
            }
            break;
        case 1: // Recycled
            for (int i=0;i<gSet->history()->recycleBin().count();i++) {
                auto *it = new QListWidgetItem(gSet->history()->recycleBin().at(i).title);
                it->setData(Qt::UserRole,1);
                it->setData(Qt::UserRole+1,i);
                helperList->addItem(it);
            }
            break;
        case 2: // History
            for (const CUrlHolder &t : qAsConst(gSet->history()->mainHistory())) {
                auto *it = new QListWidgetItem(t.title);
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

void CMainWindow::helperItemClicked(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous)

    if (current==nullptr) return;
    bool okconv = false;
    int idx = current->data(Qt::UserRole+1).toInt(&okconv);
    if (!okconv) return;
    int t = current->data(Qt::UserRole).toInt(&okconv);
    if (!okconv) return;

    QUrl u;
    QUuid uidx;
    switch (t) {
        case 0: // Tabs
            if (idx<0 || idx>=tabMain->count()) return;
            tabMain->setCurrentIndex(idx);
            break;
        case 1: // Recycled
            if (idx<0 || idx>=gSet->history()->recycleBin().count()) return;
            u = gSet->history()->recycleBin().at(idx).url;
            if (!u.isValid()) return;
            new CBrowserTab(this, u);
            gSet->history()->removeRecycledItem(idx);
            break;
        case 2: // History
            uidx = QUuid(current->data(Qt::UserRole+2).toString());
            if (!uidx.isNull()) goHistory(uidx);
            break;
        default:
            break;
    }
}

void CMainWindow::updateHistoryList()
{
    if (!(helperVisible && tabHelper->currentIndex()==2)) return;
    helperList->clear();
    for (const CUrlHolder &t : qAsConst(gSet->history()->mainHistory())) {
        auto *it = new QListWidgetItem(t.title);
        it->setStatusTip(t.url.toString());
        it->setToolTip(t.url.toString());
        it->setData(Qt::UserRole,2);
        it->setData(Qt::UserRole+1,0);
        it->setData(Qt::UserRole+2,t.uuid.toString());
        helperList->addItem(it);
    }
}

void CMainWindow::updateRecentList()
{
    recentMenu->clear();
    actionRecentDocuments->setEnabled(gSet->settings()->maxRecent>0);

    for(const QString& filename : qAsConst(gSet->history()->recentFiles())) {
        QFileInfo fi(filename);
        auto *ac = recentMenu->addAction(fi.fileName());
        ac->setToolTip(filename);
        connect(ac,&QAction::triggered,this,[this,filename](){
            openAuxFiles(QStringList() << filename);
        });
    }
}

void CMainWindow::updateTitle()
{
    QString t = QGuiApplication::applicationDisplayName();
    if (tabMain->currentWidget()) {
        auto *sv = qobject_cast<CBrowserTab*>(tabMain->currentWidget());
        if (sv!=nullptr && !sv->tabTitle().isEmpty()) {
            QTextDocument doc;
            doc.setHtml(sv->tabTitle());
            t = QSL("%1 - %2").arg(doc.toPlainText(), t);
            t.remove(u'\r');
            t.remove(u'\n');
        }

        auto *bv = qobject_cast<CSearchTab*>(tabMain->currentWidget());
        if (bv!=nullptr && !bv->getLastQuery().isEmpty())
            t = tr("[%1] search - %2").arg(bv->getLastQuery(),t);

        auto *st = qobject_cast<CSpecTabContainer*>(tabMain->currentWidget());
        if (st)
            t = tr("%1 - %2").arg(st->tabTitle(),t);
    }
    setWindowTitle(t);
}

void CMainWindow::goHistory(QUuid idx)
{
    for (const CUrlHolder& uh : qAsConst(gSet->history()->mainHistory())) {
        if (uh.uuid==idx) {
            QUrl u = uh.url;
            if (!u.isValid()) return;
            new CBrowserTab(this, u);
            break;
        }
    }
}

void CMainWindow::createSearch()
{
    new CSearchTab(this);
}

void CMainWindow::createStartBrowser()
{
    QFile f(QSL(":/data/startpage"));
    QString html;
    if (f.open(QIODevice::ReadOnly))
        html = QString::fromUtf8(f.readAll());
    new CBrowserTab(this,QUrl(),QStringList(),true,html,QSL("100%"),true);
}

void CMainWindow::checkTabs()
{
    if(tabMain->count()==0) close();
}

void CMainWindow::openAuxFiles(const QStringList &filenames)
{
    for(const QString& fname : qAsConst(filenames)) {
        if (!fname.isEmpty())
            new CBrowserTab(this, QUrl::fromLocalFile(fname));
    }
}

void CMainWindow::openAuxFileWithDialog()
{
    QStringList fnames = CGenericFuncs::getOpenFileNamesD(this,tr("Open text file"),gSet->settings()->savedAuxDir);
    if (fnames.isEmpty()) return;

    gSet->ui()->setSavedAuxDir(QFileInfo(fnames.first()).absolutePath());

    openAuxFiles(fnames);
}

void CMainWindow::openEmptyBrowser()
{
    auto *sv = new CBrowserTab(this);
    sv->urlEdit->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::openAuxFileInDir()
{
    QWidget* t = tabMain->currentWidget();
    auto *sv = qobject_cast<CBrowserTab *>(t);
    if (sv==nullptr) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Active document viewer tab not found."));
        return;
    }
    QString auxDir = sv->getUrl().toLocalFile();
    if (auxDir.isEmpty()) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Remote document opened. Cannot define local directory."));
        return;
    }
    auxDir = QFileInfo(auxDir).absolutePath();

    QStringList fnames = CGenericFuncs::getOpenFileNamesD(this,tr("Open text file"),auxDir);
    if (fnames.isEmpty()) return;

    gSet->ui()->setSavedAuxDir(QFileInfo(fnames.first()).absolutePath());

    for(int i=0;i<fnames.count();i++) {
        if (fnames.at(i).isEmpty()) continue;
        new CBrowserTab(this, QUrl::fromLocalFile(fnames.at(i)));
    }
}

void CMainWindow::createFromClipboard()
{
    QString tx = CGenericFuncs::getClipboardContent();
    if (tx.isEmpty()) {
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("Clipboard is empty or contains incompatible data."));
        return;
    }
    tx = CGenericFuncs::makeSimpleHtml(tr("Clipboard"),tx);
    auto *sv = new CBrowserTab(this, QUrl(), QStringList(), true, tx);
    sv->txtBrowser->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::createFromClipboardPlain()
{
    QString tx = CGenericFuncs::getClipboardContent(true,true);
    if (tx.isEmpty()) {
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("Clipboard is empty or contains incompatible data."));
        return;
    }
    tx = CGenericFuncs::makeSimpleHtml(tr("Clipboard plain"),tx);
    auto *sv = new CBrowserTab(this, QUrl(), QStringList(), true, tx);
    sv->txtBrowser->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::clearClipboard()
{
    QApplication::clipboard()->clear(QClipboard::Clipboard);
}

void CMainWindow::openFromClipboard()
{
    QUrl url = QUrl::fromUserInput(CGenericFuncs::getClipboardContent(true));
    url.setFragment(QString());
    QString uri = url.toString().remove(QRegularExpression(QSL("#.*$")));
    if (uri.isEmpty()) {
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("Clipboard is empty or contains incompatible data."));
        return;
    }
    auto *sv = new CBrowserTab(this, uri);
    sv->txtBrowser->setFocus(Qt::OtherFocusReason);
}

void CMainWindow::updateBookmarks()
{
    while (true) {
        const QList<QAction *> acl = menuBookmarks->actions();
        if (acl.count()>3) {
            menuBookmarks->removeAction(acl.last());
        } else {
            break;
        }
    }

    gSet->bookmarksManager()->populateBookmarksMenu(menuBookmarks, this);
}

void CMainWindow::addBookmark()
{
    if (tabMain->currentWidget()==nullptr) return;
    auto *sv = qobject_cast<CBrowserTab *>(tabMain->currentWidget());
    if (sv==nullptr) return;

    AddBookmarkDialog dlg(sv->getUrl().toString(),sv->tabTitle(),this);
    if (dlg.exec() == QDialog::Accepted)
        Q_EMIT gSet->history()->updateAllBookmarks();
}

void CMainWindow::manageBookmarks()
{
    BookmarksDialog dialog(this);
    connect(&dialog, &BookmarksDialog::openUrl,this,[this](const QUrl& url){
        new CBrowserTab(this, url);
    });

    dialog.exec();
    Q_EMIT gSet->history()->updateAllBookmarks();
}

void CMainWindow::showLightTranslator()
{
    gSet->ui()->showLightTranslator();
}

void CMainWindow::openBookmark()
{
    auto *a = qobject_cast<QAction *>(sender());
    if (a==nullptr) return;

    if (a->data().canConvert<QUrl>()) {
        QUrl u = a->data().toUrl();
        if (!u.isValid()) {
            QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                                 tr("Unable to open inconsistently loaded bookmark."));
            return;
        }

        new CBrowserTab(this, u);
    } else if (a->data().canConvert<QStringList>()) {
        QStringList sl = a->data().toStringList();
        for (const QString &s : qAsConst(sl)) {
            QUrl u(s);
            if (u.isValid())
                new CBrowserTab(this, u);
        }
    }
}

void CMainWindow::openRecycled()
{
    auto *a = qobject_cast<QAction *>(sender());
    if (a==nullptr) return;

    bool okconv = false;
    int idx = a->data().toInt(&okconv);
    if (!okconv) return;
    if (idx<0 || idx>=gSet->history()->recycleBin().count()) return;
    QUrl u = gSet->history()->recycleBin().at(idx).url;
    if (!u.isValid()) return;
    new CBrowserTab(this, u);
    gSet->history()->removeRecycledItem(idx);
}

void CMainWindow::openChromiumURLs()
{
    new CBrowserTab(this,QUrl(),QStringList(),true,CGenericFuncs::makeSpecialUrlsHtml());
}

void CMainWindow::openPixivList()
{
    const QStringList fnames = CGenericFuncs::getOpenFileNamesD(this,tr("Open Pixiv index list"),
                                                                gSet->settings()->savedAuxDir,
                                                                { tr("Json Pixiv List (*.jspix)") });
    if (fnames.isEmpty()) return;

    gSet->ui()->setSavedAuxDir(QFileInfo(fnames.first()).absolutePath());

    int errorsCount = 0;
    for(const auto& fname : fnames) {
        if (fname.isEmpty()) continue;

        QFile f(fname);
        if (!f.open(QIODevice::ReadOnly)) {
            qCritical() << tr("Unable to open file %1.").arg(fname);
            errorsCount++;
            continue;
        }

        QJsonParseError err {};
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(),&err);
        f.close();
        if (doc.isNull() || !doc.isObject()) {
            qCritical() << tr("JSON parser error for file %1: %2 at %3.")
                           .arg(fname)
                           .arg(err.error)
                           .arg(err.offset);
            errorsCount++;
            continue;
        }

        CPixivIndexTab::fromJson(this,doc.object());
    }

    if (errorsCount>0) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Some files cannot be loaded, %1 errors occured.\nCheck log output.").arg(errorsCount));
    }
}

void CMainWindow::pixivSearch()
{
    QVariantHash data;
    data[QSL("type")] = QSL("pixivList");
    data[QSL("id")] = QString();
    data[QSL("mode")] = QSL("tagSearch");
    data[QSL("base")] = QSL("novel");
    auto *ex = CAbstractExtractor::extractorFactory(data,this);
    if (ex == nullptr) return;
    if (!gSet->startup()->setupThreadedWorker(ex)) {
        delete ex;
        return;
    }

    QMetaObject::invokeMethod(ex,&CAbstractExtractor::start,Qt::QueuedConnection);
}

void CMainWindow::updateRecycled()
{
    recycledMenu->clear();
    for (int i=0;i<gSet->history()->recycleBin().count();i++) {
        QAction* a = recycledMenu->addAction(gSet->history()->recycleBin().at(i).title,this,
                                             &CMainWindow::openRecycled);
        a->setStatusTip(gSet->history()->recycleBin().at(i).url.toString());
        a->setData(i);
    }
    updateHelperList();
}

void CMainWindow::updateTabs()
{
    tabsMenu->clear();
    for(int i=0;i<tabMain->count();i++) {
        auto *sv = qobject_cast<CSpecTabContainer*>(tabMain->widget(i));
        if (sv) {
            tabsMenu->addAction(sv->tabTitle(),this,
                                &CMainWindow::activateTab)->setData(i);
        }
    }
    updateHelperList();
}

void CMainWindow::activateTab()
{
    auto *act = qobject_cast<QAction*>(sender());
    if(act==nullptr) return;
    bool okconv = false;
    int idx = act->data().toInt(&okconv);
    if (!okconv) return;
    tabMain->setCurrentIndex(idx);
}

void CMainWindow::reloadLanguagesList()
{
    menuTranslationLanguages->clear();
    menuTranslationLanguages->addActions(gSet->actions()->getTranslationLanguagesActions());
    menuSubsentencesMode->clear();
    menuSubsentencesMode->addActions(gSet->actions()->getSubsentencesModeActions());
}

void CMainWindow::reloadCharsetList()
{
    QAction* act = nullptr;
    menuCharset->clear();
    act = menuCharset->addAction(tr("Autodetect"),gSet->ui(),&CGlobalUI::forceCharset);
    act->setData(QString());
    if (gSet->settings()->forcedCharset.isEmpty()) {
        act->setCheckable(true);
        act->setChecked(true);
    } else {
        act = menuCharset->addAction(gSet->settings()->forcedCharset,gSet->ui(),&CGlobalUI::forceCharset);
        act->setData(gSet->settings()->forcedCharset);
        act->setCheckable(true);
        act->setChecked(true);
    }
    menuCharset->addSeparator();

    const QVector<QStringList> &cList = CGenericFuncs::encodingsByScript();
    for(int i=0;i<cList.count();i++) {
        QMenu* midx = menuCharset->addMenu(cList.at(i).at(0));
        for(int j=1;j<cList.at(i).count();j++) {
            const QString codec = cList.at(i).at(j);
            if (!CGenericFuncs::codecIsValid(codec)) {
                qWarning() << tr("Encoding %1 not supported.").arg(codec);
                continue;
            }
            act = midx->addAction(codec,gSet->ui(),&CGlobalUI::forceCharset);
            act->setData(codec);
            if (codec == gSet->settings()->forcedCharset) {
                act->setCheckable(true);
                act->setChecked(true);
            }
        }
    }
    menuCharset->addSeparator();

    for(const auto & cs : qAsConst(gSet->settings()->charsetHistory)) {
        if (cs==gSet->settings()->forcedCharset) continue;
        act = menuCharset->addAction(cs,gSet->ui(),&CGlobalUI::forceCharset);
        act->setData(cs);
        act->setCheckable(true);
    }
}

void CMainWindow::closeEvent(QCloseEvent *event)
{
    Q_EMIT aboutToClose(this);
    event->accept();
}

bool CMainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    const int fullscreenToolsShowMargin = 20;

    if (fullScreen) {
        if (ev->type()==QEvent::MouseMove) {
            auto *mev = dynamic_cast<QMouseEvent *>(ev);
            if (mev) {
                if (mev->y()<fullscreenToolsShowMargin && !tabMain->tabBar()->isVisible()) {
                    setToolsVisibility(true);
                } else if (mev->y()>(fullscreenToolsShowMargin+4) && tabMain->tabBar()->isVisible()) {
                    setToolsVisibility(false);
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj,ev);
}

void CMainWindow::dragEnterEvent(QDragEnterEvent *ev)
{
    QStringList formats = ev->mimeData()->formats();

    if (formats.contains(QSL("_NETSCAPE_URL")) ||
            formats.contains(QSL("text/uri-list"))) {
        ev->acceptProposedAction();

    } else if (formats.contains(QSL("text/plain"))) {
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
        data[formats.at(i)] = CGenericFuncs::detectDecodeToUnicode(ev->mimeData()->data(formats.at(i)));

    QVector<QUrl> ul;
    bool ok = false;

    if (data.contains(QSL("_NETSCAPE_URL"))) {
        QString s = data.value(QSL("_NETSCAPE_URL"))
                    .split(QRegularExpression(QSL("\n|\r\n|\r"))).constFirst();
        QUrl u(s);
        if (u.isValid()) {
            ul << u;
            ok = true;
        }
    }
    if (!ok && data.contains(QSL("text/uri-list"))) {
        QStringList sl = data.value(QSL("text/uri-list"))
                         .split(QRegularExpression(QSL("\n|\r\n|\r")));
        for (int i=0;i<sl.count();i++) {
            if (sl.at(i).startsWith(u'#')) continue;
            QUrl u(sl.at(i));
            if (u.isValid()) {
                ul << u;
                ok = true;
            }
        }
    }
    if (!ok && data.contains(QSL("text/plain"))) {
        QUrl u(data.value(QSL("text/plain"))
               .split(QRegularExpression(QSL("\n|\r\n|\r"))).constFirst());
        if (u.isValid())
            ul << u;
    }

    for (const QUrl& u : qAsConst(ul)) {
        auto *sv = new CBrowserTab(this, u);
        sv->txtBrowser->setFocus(Qt::OtherFocusReason);
    }
}

void CMainWindow::helpAbout()
{
    QString recoll = tr("no");
    QString baloo5 = tr("no");
    QString poppler = tr("no");
    QString srchilite = tr("no");
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
#ifdef WITH_SRCHILITE
    srchilite = tr("yes");
#endif

    QString msg = tr("JPReader.\nFor assisted text searching, translating and reading\n\n"
                     "Build: %1 %2\n"
                     "Platform: %3\n"
                     "Build date: %4\n\n"
                     "Recoll backend compiled: %5\n"
                     "Baloo backend compiled: %6\n"
                     "Poppler support: %7\n"
                     "Source-highlight: %8.")
                  .arg(QSL(BUILD_REV),
                       debugstr,
                       QSL(BUILD_PLATFORM),
                       QSL(BUILD_DATE),
                       recoll,
                       baloo5,
                       poppler,
                       srchilite);

    QMessageBox::about(this, QGuiApplication::applicationDisplayName(), msg);
}
