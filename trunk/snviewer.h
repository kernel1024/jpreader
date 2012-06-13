#ifndef SNVIEWER_H
#define SNVIEWER_H 1

#include <QtCore>
#include <QtGui>

#include <QtWebKit>
#include <klineedit.h>
#include <khistorycombobox.h>
#include "ui_snviewer.h"
#include "waitdlg.h"
#include "calcthread.h"
#include "mainwindow.h"
#include "specwidgets.h"

#include "snctxhandler.h"
#include "snmsghandler.h"
#include "snnet.h"
#include "sntrans.h"

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
public slots:
    void closeTab(bool nowait = false);
    void recycleTab();
    void navByUrl(QString url);
    void titleChanged(const QString & title);
    void urlChanged(const QUrl & url);
    void statusBarMsg(const QString & msg);
};

#endif
