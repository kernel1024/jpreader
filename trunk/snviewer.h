#ifndef SNVIEWER_H
#define SNVIEWER_H 1

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QWebFrame>
#include <QList>
#include <QUrl>
#include <QKeyEvent>
#include <QTimer>

#include "ui_snviewer.h"
#include "waitdlg.h"
#include "calcthread.h"
#include "mainwindow.h"
#include "specwidgets.h"

#include "snctxhandler.h"
#include "snmsghandler.h"
#include "snnet.h"
#include "sntrans.h"
#include "dictionary_interface.h"

class CSnCtxHandler;
class CSnMsgHandler;
class CSnNet;
class CSnTrans;

class CSnippetViewer : public QWidget, public Ui::SnippetViewer
{
	Q_OBJECT
public:
    CMainWindow* parentWnd;
    CSnCtxHandler *ctxHandler;
    CSnMsgHandler *msgHandler;
    CSnNet *netHandler;
    CSnTrans *transHandler;

    QStringList slist;
    CWaitDlg* waitDlg;
    OrgQjradDictionaryInterface* dbusDict;
	QString calculatedUrl;
	QSpecTabWidget* tabWidget;
	QPointer<QWebFrame> lastFrame;
	bool fileChanged;
	QUrl savedBaseUrl;
    QString auxContent;
    QUrl firstUri;
    QUrl Uri;
    QString tabTitle;
    bool translationBkgdFinished;
    bool loadingBkgdFinished;
    bool isStartPage;
    QList<QUrl> backHistory;
    QList<QUrl> forwardStack;
    QTimer *selectionTimer;
    QString storedSelection;

    CSnippetViewer(CMainWindow* parent, QUrl aUri = QUrl(), QStringList aSearchText = QStringList(),
                   bool setFocused = true, QString AuxContent="", QString zoom = QString("100%"),
                   bool startPage = false);
    virtual ~CSnippetViewer();
    QString getDocTitle();
    void bindToTab(QSpecTabWidget* tabs, bool setFocused = true);
    bool onceLoaded;
    bool onceTranslated;
	bool loadingByWebKit;
    void keyPressEvent(QKeyEvent* event);
    void updateButtonsState();
    void updateWebViewAttributes();
    void updateHistorySuggestion(const QStringList &suggestionList);
public slots:
    void closeTab(bool nowait = false);
    void recycleTab();
    void navByUrl(QString url);
    void navByUrl();
    void titleChanged(const QString & title);
    void urlChanged(const QUrl & url);
    void selectionChanged();
    void selectionShow();
    void hideTooltip();
    void statusBarMsg(const QString & msg);
    void showWordTranslation(const QString & html);
};

#endif
