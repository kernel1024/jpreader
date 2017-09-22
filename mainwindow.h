#ifndef MAINWINDOW_H
#define MAINWINDOW_H 1

#include <QMainWindow>
#include <QLabel>
#include <QUuid>
#include <QMenu>
#include <QListWidgetItem>
#include <QPoint>
#include <QCloseEvent>
#include <QTimer>
#include "ui_main.h"

class CSnippetViewer;

class CMainWindow : public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT
public:
    CMainWindow(bool withSearch = false, bool withViewer = true,
                const QUrl &withViewerUrl = QUrl());
    virtual ~CMainWindow();
	int lastTabIdx;

    // -------- Global settings and objects --------
    QLabel stSearchStatus;
    QTimer titleRenamedLock;

    // -------------------------------------

    void goHistory(QUuid idx);
    void updateTitle();
    void checkTabs();

    // Menu updaters
    void reloadCharsetList();
    void updateBookmarks();
    void updateQueryHistory();
    void updateRecycled();
    void updateTabs();
    void updateHistoryList();
    void updateRecentList();

    CSnippetViewer *getOpenedInspectorTab();
private:
    QMenu* recycledMenu;
    QMenu* tabsMenu;
    QMenu* recentMenu;
    CSpecTabBar* tabHelper;
    int savedHelperIdx;
    int savedHelperWidth;
    int savedSplitterWidth;
    QPoint savedPos;
    QSize savedSize;
    bool fullScreen;
    bool savedMaximized;
    bool helperVisible;

    void updateHelperList();

public slots:
	void helpAbout();

	void openBookmark();
    void openAllBookmarks();
    void openAuxFiles(const QStringList &filenames);
    void openAuxFileWithDialog();
    void openAuxFileInDir();
    void openEmptyBrowser();
    void openRecycled();
	void tabChanged(int idx);
    void createFromClipboard();
    void createFromClipboardPlain();
    void clearClipboard();
    void openFromClipboard();
    void forceCharset();
    void createSearch();
    void createStartBrowser();
    void activateTab();
    void helperClicked(int index);
    void helperPreClicked(int index);
    void helperItemClicked(QListWidgetItem* current, QListWidgetItem* previous);
    void splitterMoved(int pos, int index);
    void addBookmark();
    void showLightTranslator();
    void closeStartPage();
    void tabBarTooltip(const QPoint& globalPos, const QPoint &localPos);
    void detachTab();
    void findText();
    void switchFullscreen();
    void setToolsVisibility(bool visible);
    void printToPDF();
    void save();
signals:
    void aboutToClose(CMainWindow* sender);

protected:
    void centerWindow();
    void updateSplitters();
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *ev);
    void dragEnterEvent(QDragEnterEvent *ev);
    void dropEvent(QDropEvent *ev);
};

#endif
