#ifndef SNVIEWER_H
#define SNVIEWER_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QList>
#include <QUrl>
#include <QKeyEvent>
#include <QTimer>
#include <QWebEngineView>

#include "specwidgets.h"
#include "ui_snviewer.h"

class CSnCtxHandler;
class CSnMsgHandler;
class CSnNet;
class CSnTrans;
class CSnWaitCtl;

class CSnippetViewer : public CSpecTabContainer, public Ui::SnippetViewer
{
	Q_OBJECT
public:
    CSnCtxHandler *ctxHandler;
    CSnMsgHandler *msgHandler;
    CSnNet *netHandler;
    CSnTrans *transHandler;
    CSnWaitCtl *waitHandler;
    QPixmap pageImage;

    QStringList searchList;
	QString calculatedUrl;
	bool fileChanged;
    bool translationBkgdFinished;
    bool loadingBkgdFinished;
    bool isStartPage;
    bool onceTranslated;
    bool loading;
    bool requestAutotranslate;
    bool pageLoaded;
    bool auxContentLoaded;

    explicit CSnippetViewer(CMainWindow* parent, const QUrl& aUri = QUrl(),
                            const QStringList& aSearchText = QStringList(),
                            bool setFocused = true, const QString& AuxContent = QString(),
                            const QString& zoom = QString("100%"),
                            bool startPage = false);
    virtual QString getDocTitle();
    void keyPressEvent(QKeyEvent* event) override;
    void updateButtonsState();
    void updateWebViewAttributes();
    bool canClose() override;
    void recycleTab() override;
    QUrl getUrl();
    void setToolbarVisibility(bool visible) override;
    void outsideDragStart() override;
    bool isInspector();
    void printToPDF();
public slots:
    void navByUrlDefault();
    void navByUrl(const QUrl &url);
    void navByUrl(const QString &url);
    void titleChanged(const QString & title);
    void urlChanged(const QUrl & url);
    void statusBarMsg(const QString & msg);
    void takeScreenshot();
};

#endif
