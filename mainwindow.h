#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QUuid>
#include <QMenu>
#include <QListWidgetItem>
#include <QPoint>
#include <QCloseEvent>
#include <QTimer>
#include "ui_main.h"

class CMainWindow : public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT
public:
    explicit CMainWindow(bool withSearch = false, bool withViewer = true,
                const QVector<QUrl> &viewerUrls = { }, QWidget *parent = nullptr);
    ~CMainWindow() override = default;

    void goHistory(QUuid idx);
    void updateTitle();
    void checkTabs();
    void updateTabs();
    void setSearchStatus(const QString& text);
    void startTitleRenameTimer();
    bool isTitleRenameTimerActive() const;
    int lastTabIndex() const;
    bool sendInputToActiveBrowser(const QString& text);

private:
    QMenu* recycledMenu;
    QMenu* tabsMenu;
    QMenu* recentMenu;
    QLabel* stSearchStatus;
    CSpecTabBar* tabHelper;
    int savedHelperIdx { -1 };
    int savedHelperWidth { 0 };
    int savedSplitterWidth { 0 };
    int lastTabIdx { 0 };
    QPoint savedPos;
    QSize savedSize;
    bool fullScreen { false };
    bool savedMaximized { false };
    bool helperVisible { false };
    QTimer titleRenamedLock;

    Q_DISABLE_COPY(CMainWindow)

    void updateHelperList();

public Q_SLOTS:
	void helpAbout();
	void openBookmark();
    void openAuxFiles(const QStringList &filenames);
    void openAuxFileWithDialog();
    void openAuxFileInDir();
    void openEmptyBrowser();
    void openRecycled();
    void openChromiumURLs();
    void openPixivList();
    void pixivSearch();
    void tabChanged(int idx);
    void createFromClipboard();
    void createFromClipboardPlain();
    void clearClipboard();
    void openFromClipboard();
    void createSearch();
    void createStartBrowser();
    void activateTab();
    void helperClicked(int index);
    void helperPreClicked(int index);
    void helperItemClicked(QListWidgetItem* current, QListWidgetItem* previous);
    void splitterMoved(int pos, int index);
    void addBookmark();
    void manageBookmarks();
    void showLightTranslator();
    void closeStartPage();
    void tabBarTooltip(const QPoint& globalPos, const QPoint &localPos);
    void detachTab();
    void findText();
    void switchFullscreen();
    void setToolsVisibility(bool visible);
    void printToPDF();
    void save();
    void displayStatusBarMessage(const QString& text);

    // Menu updaters
    void updateBookmarks();
    void updateRecentList();
    void reloadCharsetList();
    void updateHistoryList();
    void updateQueryHistory();
    void updateRecycled();
    void reloadLanguagesList();

Q_SIGNALS:
    void aboutToClose(CMainWindow* sender);

protected:
    void centerWindow();
    void updateSplitters();
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void dragEnterEvent(QDragEnterEvent *ev) override;
    void dropEvent(QDropEvent *ev) override;

};

#endif
