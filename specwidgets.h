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

#ifdef WEBENGINE_56
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>
#endif

class CMainWindow;
class CSnippetViewer;

class QSpecTabBar : public QTabBar {
	Q_OBJECT
public:
    QSpecTabBar(QWidget *p = 0);
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

class QSpecTabWidget : public QTabWidget {
	Q_OBJECT
public:
	QSpecTabWidget(QWidget *p = 0);
    QSpecTabBar *tabBar() const;
    CMainWindow* parentWnd;
    bool mainTabWidget;

private:
    QSpecTabBar* m_tabBar;

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

class QSpecMenuStyle : public QProxyStyle {
    Q_OBJECT
public:
    int styleHint ( StyleHint hint, const QStyleOption * option = 0, const QWidget * widget = 0,
                            QStyleHintReturn * returnData = 0 ) const;
};

class QSpecToolTipLabel : public QLabel {
    Q_OBJECT
public:
    QSpecToolTipLabel(const QString &text = QString());
private:
    void hideEvent(QHideEvent *);
signals:
    void labelHide();
};

class QHotkeyEdit : public QLineEdit {
    Q_OBJECT
public:
    QHotkeyEdit(QWidget *parent);
    QKeySequence keySequence() const;
    void setKeySequence(const QKeySequence &sequence);
protected:
    QKeySequence p_shortcut;
    void keyPressEvent(QKeyEvent *event);
    void updateSequenceView();
};

class QSpecTabContainer : public QWidget {
    Q_OBJECT
public:
    CMainWindow* parentWnd;
    QSpecTabWidget* tabWidget;
    QString tabTitle;
    QSpecTabContainer(CMainWindow *parent = 0);
    void bindToTab(QSpecTabWidget *tabs, bool setFocused = true);

    virtual bool canClose() { return true; }
    virtual void recycleTab() { }
    virtual QString getDocTitle() { return QString(); }
    virtual void setToolbarVisibility(bool visible) { Q_UNUSED(visible); }
    void updateTabIcon(const QIcon& icon);
public slots:
    void detachTab();
    void closeTab(bool nowait = false);
};

class QSpecWebPage : public QWebEnginePage {
    Q_OBJECT
private:
    CSnippetViewer* parentViewer;
public:
    QSpecWebPage(CSnippetViewer* parent);
    QSpecWebPage(QWebEngineProfile* profile, CSnippetViewer* parent);
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame);
    bool certificateError(const QWebEngineCertificateError &certificateError);
    QWebEnginePage* createWindow(WebWindowType type);
signals:
    void linkClickedExt(const QUrl& url, const int type, const bool isMainFrame);
};

class QSpecLogHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    QSpecLogHighlighter(QTextDocument* parent);
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

class QSpecUrlInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    QSpecUrlInterceptor(QObject *p = 0);
    bool interceptRequest(QWebEngineUrlRequestInfo &info);
};

class QSpecGDSchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT
public:
    QSpecGDSchemeHandler(const QByteArray& scheme, QObject* parent = 0);
    void requestStarted(QWebEngineUrlRequestJob * request);
    QByteArray scheme() const;
};

#endif // WEBENGINE_56

class QSpecUrlHistoryModel : public QAbstractListModel {
    Q_OBJECT
public:
    QSpecUrlHistoryModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
};

#endif // SPECTABWIDGET_H
