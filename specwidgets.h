#ifndef SPECTABWIDGET_H
#define SPECTABWIDGET_H

#include <QWidget>
#include <QTabBar>
#include <QMouseEvent>
#include <QTabWidget>
#include <QEvent>
#include <QPoint>
#include <QProxyStyle>
#include <QStyleOption>
#include <QUrl>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QLabel>
#include <QHideEvent>
#include <QWebEnginePage>
#include <QSyntaxHighlighter>
#include <QAbstractListModel>
#include <QEventLoop>
#include <QTextBrowser>
#include <QNetworkCookieJar>
#include <QWebEngineView>

#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>

class CMainWindow;
class CSnippetViewer;
class CSpecTabWidget;
class CSpecTabContainer;

class CSpecTabBar : public QTabBar {
	Q_OBJECT
public:
    CSpecTabBar(CSpecTabWidget *p);
    CSpecTabBar(QWidget *p = nullptr);
    void setBrowserTabs(bool enabled);
private:
    CSpecTabWidget *m_tabWidget;
    int m_browserTabs;
    CSpecTabContainer* m_draggingTab;
    QPoint m_dragStart;
protected:
    void mousePressEvent( QMouseEvent * event );
    void mouseReleaseEvent( QMouseEvent * event );
    void mouseMoveEvent( QMouseEvent * event);
    QSize minimumTabSizeHint(int index) const;
signals:
	void tabRightClicked(int index);
    void tabLeftClicked(int index);
    void tabRightPostClicked(int index);
    void tabLeftPostClicked(int index);
};

class CSpecTabWidget : public QTabWidget {
	Q_OBJECT
public:
    CSpecTabWidget(QWidget *p = nullptr);
    CSpecTabBar *tabBar() const;
    CMainWindow* parentWnd;
    bool mainTabWidget;

private:
    CSpecTabBar* m_tabBar;

protected:
    void mouseDoubleClickEvent( QMouseEvent * event );
    bool event(QEvent * event );

public slots:
	void tabRightClick(int index);
    void tabLeftClick(int index);
    void createTab();
    void selectPrevTab();
    void selectNextTab();
signals:
    void tabRightClicked(int index);
    void tabLeftClicked(int index);
    void tooltipRequested(const QPoint& globalPos, const QPoint& localPos);
};

class CSpecMenuStyle : public QProxyStyle {
    Q_OBJECT
public:
    int styleHint ( StyleHint hint, const QStyleOption * option = 0, const QWidget * widget = 0,
                            QStyleHintReturn * returnData = 0 ) const;
};

class CSpecToolTipLabel : public QLabel {
    Q_OBJECT
public:
    CSpecToolTipLabel(const QString &text = QString());
private:
    void hideEvent(QHideEvent *);
signals:
    void labelHide();
};

class CSpecTabContainer : public QWidget {
    Q_OBJECT
public:
    CMainWindow* parentWnd;
    CSpecTabWidget* tabWidget;
    QString tabTitle;
    CSpecTabContainer(CMainWindow *parent = nullptr);
    void bindToTab(CSpecTabWidget *tabs, bool setFocused = true);

    virtual bool canClose() { return true; }
    virtual void recycleTab() { }
    virtual QString getDocTitle() { return QString(); }
    virtual void setToolbarVisibility(bool visible) { Q_UNUSED(visible); }
    virtual void outsideDragStart() { }
public slots:
    void detachTab();
    void closeTab(bool nowait = false);
    void updateTabIcon(const QIcon& icon);
};

class CSpecWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    CSpecWebPage(CSnippetViewer* parent);
    CSpecWebPage(QWebEngineProfile* profile, CSnippetViewer* parent);
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame);
    bool certificateError(const QWebEngineCertificateError &certificateError);
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int lineNumber, const QString &sourceID);
signals:
    void linkClickedExt(const QUrl& url, int type, bool isMainFrame);
};

class CSpecWebView : public QWebEngineView {
    Q_OBJECT
private:
    CSnippetViewer* parentViewer;
    CSpecWebPage* m_page;
public:
    CSpecWebView(QWidget* parent);
    CSpecWebView(CSnippetViewer* parent);
    CSpecWebPage* customPage() const;
protected:
    QWebEngineView* createWindow(QWebEnginePage::WebWindowType type);
    void contextMenuEvent(QContextMenuEvent* event);

};

class CSpecLogHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    CSpecLogHighlighter(QTextDocument* parent);
protected:
    void highlightBlock(const QString& text);
private:
    void formatBlock(const QString& text,
                     const QRegExp& exp,
                     const QColor& color = Qt::black,
                     bool weight = false,
                     bool italic = false,
                     bool underline = false,
                     bool strikeout = false);
};

class CSpecUrlInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    CSpecUrlInterceptor(QObject *p = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info);
};

class CSpecUrlHistoryModel : public QAbstractListModel {
    Q_OBJECT
public:
    CSpecUrlHistoryModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
};

class CGDTextBrowser : public QTextBrowser
{
    Q_OBJECT
public:
    explicit CGDTextBrowser(QWidget* parent = nullptr);

protected:
    QVariant loadResource(int type, const QUrl &url);

};

class CIOEventLoop : public QEventLoop {
    Q_OBJECT
public:
    CIOEventLoop(QObject* parent = nullptr);

public slots:
    void finished();
    void timeout();
};

class CNetworkCookieJar : public QNetworkCookieJar
{
    Q_OBJECT
public:
    explicit CNetworkCookieJar(QObject * parent = nullptr);

    QList<QNetworkCookie> getAllCookies();
    void initAllCookies(const QList<QNetworkCookie> & cookies);
};

class CFaviconLoader : public QObject
{
    Q_OBJECT
private:
    QUrl m_url;
public:
    explicit CFaviconLoader(QObject *parent, const QUrl &url);
public slots:
    void queryStart(bool forceCached);
    void queryFinished();
signals:
    void gotIcon(const QIcon& icon);
};


#endif // SPECTABWIDGET_H
