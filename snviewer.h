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
#include "translator.h"
#include "mainwindow.h"
#include "specwidgets.h"

#include "snctxhandler.h"
#include "snmsghandler.h"
#include "snnet.h"
#include "sntrans.h"
#include "snwaitctl.h"

class CSnCtxHandler;
class CSnMsgHandler;
class CSnNet;
class CSnTrans;
class CSnWaitCtl;

class CSnippetViewer : public QSpecTabContainer, public Ui::SnippetViewer
{
	Q_OBJECT
public:
    CSnCtxHandler *ctxHandler;
    CSnMsgHandler *msgHandler;
    CSnNet *netHandler;
    CSnTrans *transHandler;
    CSnWaitCtl *waitHandler;

    QStringList slist;
	QString calculatedUrl;
	QPointer<QWebFrame> lastFrame;
	bool fileChanged;
	QUrl savedBaseUrl;
    QString auxContent;
    QUrl firstUri;
    QUrl Uri;
    bool translationBkgdFinished;
    bool loadingBkgdFinished;
    bool isStartPage;
    QList<QUrl> backHistory;
    QList<QUrl> forwardStack;
    bool onceLoaded;
    bool onceTranslated;
    bool loadingByWebKit;
    bool requestAutotranslate;

    CSnippetViewer(CMainWindow* parent, QUrl aUri = QUrl(), QStringList aSearchText = QStringList(),
                   bool setFocused = true, QString AuxContent="", QString zoom = QString("100%"),
                   bool startPage = false);
    virtual QString getDocTitle();
    void keyPressEvent(QKeyEvent* event);
    void updateButtonsState();
    void updateWebViewAttributes();
    void updateHistorySuggestion(const QStringList &suggestionList);
    virtual bool canClose();
    virtual void recycleTab();
public slots:
    void navByUrl(QString url);
    void navByUrl();
    void titleChanged(const QString & title);
    void urlChanged(const QUrl & url);
    void statusBarMsg(const QString & msg);
};

#endif
