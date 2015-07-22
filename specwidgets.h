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

class CMainWindow;
class CSnippetViewer;

class QSpecTabBar : public QTabBar {
	Q_OBJECT
public:
	QSpecTabBar(QWidget *p = 0);
protected:
	virtual void mousePressEvent( QMouseEvent * event );
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

public:
	QTabBar *tabBar() const;
    CMainWindow* parentWnd;
    bool mainTabWidget;

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
public slots:
    void detachTab();
    void closeTab(bool nowait = false);
};

class QSpecWebPage : public QWebEnginePage {
    Q_OBJECT
private:
    CMainWindow* parentWnd;
public:
    QSpecWebPage(QObject* parent = 0);
    QSpecWebPage(QWebEngineProfile* profile, CMainWindow* wnd, QObject* parent = 0);
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame);
    bool certificateError(const QWebEngineCertificateError &certificateError);
    QWebEnginePage* createWindow(WebWindowType type);
signals:
    void linkClickedExt(const QUrl& url, NavigationType type);
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

#endif // SPECTABWIDGET_H
