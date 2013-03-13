#ifndef SPECTABWIDGET_H
#define SPECTABWIDGET_H

#include <QtCore>
#include <QtGui>
#include <QtWebKit>
#include <QtNetwork>

class CMainWindow;

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

class QSpecWebView : public QWebView {
    Q_OBJECT
public:
    CMainWindow* parentWnd;
    QSpecWebView(QWidget *parent, CMainWindow *mainWnd);
protected:
    virtual QWebView* createWindow(QWebPage::WebWindowType type);
};

class QSpecCookieJar : public QNetworkCookieJar {
    Q_OBJECT
public:
    QSpecCookieJar(QWidget *parent = 0);
    void loadCookies(QByteArray* dump);
    QByteArray saveCookies();
    void clearCookies();
};

class QSpecMenuStyle : public QProxyStyle {
    Q_OBJECT
public:
    int styleHint ( StyleHint hint, const QStyleOption * option = 0, const QWidget * widget = 0,
                            QStyleHintReturn * returnData = 0 ) const;
};

class QSpecWebPage : public QWebPage {
    Q_OBJECT
protected:
    bool acceptNavigationRequest( QWebFrame * frame, const QNetworkRequest & request, NavigationType type );
signals:
    void linkClickedExt(QWebFrame * frame, const QUrl &url);
};

class QSpecNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
protected:
    QMap<QUrl,QVariant> cachePolicy;
    virtual QNetworkReply * createRequest(Operation op,
                                          const QNetworkRequest & req,
                                          QIODevice * outgoingData = 0);
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

#endif // SPECTABWIDGET_H
