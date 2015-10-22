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
#include <QIODevice>
#include <QString>
#include <QLabel>
#include <QHideEvent>
#include <QLineEdit>
#include <QKeySequence>
#include <QKeyEvent>
#include <QWebEnginePage>
#include <QSyntaxHighlighter>
#include <QAbstractListModel>
#include <QEventLoop>

#ifdef WEBENGINE_56
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>
#endif

class CMainWindow;
class CSnippetViewer;

class CSpecTabBar : public QTabBar {
	Q_OBJECT
public:
    CSpecTabBar(QWidget *p = 0);
    void setBrowserTabs(bool enabled);
private:
    int m_browserTabs;
protected:
	virtual void mousePressEvent( QMouseEvent * event );
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
    CSpecTabWidget(QWidget *p = 0);
    CSpecTabBar *tabBar() const;
    CMainWindow* parentWnd;
    bool mainTabWidget;

private:
    CSpecTabBar* m_tabBar;

protected:
    virtual void mouseDoubleClickEvent( QMouseEvent * event );
    virtual bool event(QEvent * event );

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

class CHotkeyEdit : public QLineEdit {
    Q_OBJECT
public:
    CHotkeyEdit(QWidget *parent);
    QKeySequence keySequence() const;
    void setKeySequence(const QKeySequence &sequence);
protected:
    QKeySequence p_shortcut;
    void keyPressEvent(QKeyEvent *event);
    void updateSequenceView();
};

class CSpecTabContainer : public QWidget {
    Q_OBJECT
public:
    CMainWindow* parentWnd;
    CSpecTabWidget* tabWidget;
    QString tabTitle;
    CSpecTabContainer(CMainWindow *parent = 0);
    void bindToTab(CSpecTabWidget *tabs, bool setFocused = true);

    virtual bool canClose() { return true; }
    virtual void recycleTab() { }
    virtual QString getDocTitle() { return QString(); }
    virtual void setToolbarVisibility(bool visible) { Q_UNUSED(visible); }
    void updateTabIcon(const QIcon& icon);
public slots:
    void detachTab();
    void closeTab(bool nowait = false);
};

class CSpecWebPage : public QWebEnginePage {
    Q_OBJECT
private:
    CSnippetViewer* parentViewer;
public:
    CSpecWebPage(CSnippetViewer* parent);
    CSpecWebPage(QWebEngineProfile* profile, CSnippetViewer* parent);
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame);
    bool certificateError(const QWebEngineCertificateError &certificateError);
    QWebEnginePage* createWindow(WebWindowType type);
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int lineNumber, const QString &sourceID);
signals:
    void linkClickedExt(const QUrl& url, const int type, const bool isMainFrame);
};

class CSpecLogHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    CSpecLogHighlighter(QTextDocument* parent);
protected:
    virtual void highlightBlock(const QString& text);
private:
    void formatBlock(const QString& text,
                     const QRegExp& exp,
                     const QColor& color = Qt::black,
                     bool weight = false,
                     bool italic = false,
                     bool underline = false,
                     bool strikeout = false);
};

#ifdef WEBENGINE_56

class CSpecUrlInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    CSpecUrlInterceptor(QObject *p = 0);
    bool interceptRequest(QWebEngineUrlRequestInfo &info);
};

class CSpecGDSchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT
public:
    CSpecGDSchemeHandler(QObject* parent = 0);
    void requestStarted(QWebEngineUrlRequestJob * request);
};

#endif // WEBENGINE_56

class CSpecUrlHistoryModel : public QAbstractListModel {
    Q_OBJECT
public:
    CSpecUrlHistoryModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
};

class CMemFile : public QIODevice {
    Q_OBJECT
public:
    CMemFile(const QByteArray &fileData);

    virtual qint64 bytesAvailable() const;
    virtual void close();

protected:
    virtual qint64 readData(char *data, qint64 maxlen);
    virtual qint64 writeData(const char *buffer, qint64 maxlen);

private:
    QByteArray data;
    const qint64 origLen;
};

class CIOEventLoop : public QEventLoop {
    Q_OBJECT
public:
    CIOEventLoop(QObject* parent = 0);

public slots:
    void finished();
    void timeout();
    void objDestroyed(QObject *obj);
};

class CWebHitTestResult {
private:
public:
    CWebHitTestResult();
    CWebHitTestResult &operator=(const CWebHitTestResult &other);

    bool isNull() const;

    QPoint pos;
    QString title;
    QString tagName;
    QUrl linkUrl;
    QUrl imageUrl;
};

Q_DECLARE_METATYPE(CWebHitTestResult)

#endif // SPECTABWIDGET_H
