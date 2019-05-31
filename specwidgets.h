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
private:
    CSpecTabWidget *m_tabWidget;
    bool m_browserTabs { false };
    CSpecTabContainer* m_draggingTab;
    QPoint m_dragStart;

public:
    explicit CSpecTabBar(CSpecTabWidget *p);
    explicit CSpecTabBar(QWidget *p = nullptr);
    void setBrowserTabs(bool enabled);

protected:
    void mousePressEvent( QMouseEvent * event ) override;
    void mouseReleaseEvent( QMouseEvent * event ) override;
    void mouseMoveEvent( QMouseEvent * event) override;
    QSize minimumTabSizeHint(int index) const override;

Q_SIGNALS:
	void tabRightClicked(int index);
    void tabLeftClicked(int index);
    void tabRightPostClicked(int index);
    void tabLeftPostClicked(int index);
};

class CSpecTabWidget : public QTabWidget {
	Q_OBJECT
private:
    CMainWindow* m_parentWnd { nullptr };
    bool mainTabWidget { true };
    CSpecTabBar* m_tabBar { nullptr };

public:
    explicit CSpecTabWidget(QWidget *p = nullptr);
    CSpecTabBar *tabBar() { return m_tabBar; }
    void setParentWnd(CMainWindow* wnd) { m_parentWnd = wnd; }
    CMainWindow* parentWnd() { return m_parentWnd; }

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;

public Q_SLOTS:
	void tabRightClick(int index);
    void tabLeftClick(int index);
    void createTab();
    void selectPrevTab();
    void selectNextTab();

Q_SIGNALS:
    void tabRightClicked(int index);
    void tabLeftClicked(int index);
    void tooltipRequested(const QPoint& globalPos, const QPoint& localPos);
};

class CSpecMenuStyle : public QProxyStyle {
    Q_OBJECT
public:
    explicit CSpecMenuStyle(QStyle *style = nullptr);
    int styleHint ( StyleHint hint,
                    const QStyleOption * option = nullptr,
                    const QWidget * widget = nullptr,
                    QStyleHintReturn * returnData = nullptr ) const override;
};

class CSpecToolTipLabel : public QLabel {
    Q_OBJECT
public:
    explicit CSpecToolTipLabel(const QString &text = QString(), QWidget *parent = nullptr);
private:
    void hideEvent(QHideEvent *event) override;
};

class CSpecTabContainer : public QWidget {
    Q_OBJECT
private:
    CMainWindow* m_parentWnd { nullptr };
    CSpecTabWidget* m_tabWidget { nullptr };
    QString m_tabTitle;

public:
    explicit CSpecTabContainer(QWidget *parent = nullptr);
    void bindToTab(CSpecTabWidget *tabs, bool setFocused = true);

    virtual bool canClose() { return true; }
    virtual void recycleTab() { }
    virtual void setToolbarVisibility(bool visible) { Q_UNUSED(visible) }
    virtual void outsideDragStart() { }

    CMainWindow* parentWnd() { return m_parentWnd; }
    CSpecTabWidget* tabWidget() { return m_tabWidget; }
    QString tabTitle() const { return m_tabTitle; }
    void setTabTitle(const QString& title);

public Q_SLOTS:
    void detachTab();
    void closeTab(bool nowait = false);
    void updateTabIcon(const QIcon& icon);
};

class CSpecWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit CSpecWebPage(QObject* parent);
    CSpecWebPage(QWebEngineProfile* profile, QObject* parent);
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    bool certificateError(const QWebEngineCertificateError &certificateError) override;
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int lineNumber, const QString &sourceID) override;
Q_SIGNALS:
    void linkClickedExt(const QUrl& url, int type, bool isMainFrame);
};

class CSpecWebView : public QWebEngineView {
    Q_OBJECT
private:
    CSnippetViewer* parentViewer;
    CSpecWebPage* m_page;
public:
    explicit CSpecWebView(QWidget* parent);
    explicit CSpecWebView(CSnippetViewer* parent);
    CSpecWebPage* customPage() const;
protected:
    QWebEngineView* createWindow(QWebEnginePage::WebWindowType type) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
Q_SIGNALS:
    void contextMenuRequested(const QPoint &pos, const QWebEngineContextMenuData &data);
};

class CSpecLogHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit CSpecLogHighlighter(QTextDocument* parent);
protected:
    void highlightBlock(const QString& text) override;
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
    explicit CSpecUrlInterceptor(QObject *p = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
};

class CSpecUrlHistoryModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit CSpecUrlHistoryModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
};

class CGDTextBrowser : public QTextBrowser
{
    Q_OBJECT
public:
    explicit CGDTextBrowser(QWidget* parent = nullptr);

protected:
    QVariant loadResource(int type, const QUrl &url) override;

};

class CIOEventLoop : public QEventLoop {
    Q_OBJECT
public:
    explicit CIOEventLoop(QObject* parent = nullptr);

public Q_SLOTS:
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
public Q_SLOTS:
    void queryStart(bool forceCached);
    void queryFinished();
Q_SIGNALS:
    void gotIcon(const QIcon& icon);
};


#endif // SPECTABWIDGET_H
