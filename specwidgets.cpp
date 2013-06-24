#include <QNetworkCookie>

#include "specwidgets.h"
#include "snviewer.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "searchtab.h"

QSpecTabWidget::QSpecTabWidget(QWidget *p)
	: QTabWidget(p)
{
    parentWnd = NULL;
    mainTabWidget = true;
    QSpecTabBar* tb = new QSpecTabBar(this);
    setTabBar(tb);
    connect(tb,SIGNAL(tabRightClicked(int)),this,SLOT(tabRightClick(int)));
    connect(tb,SIGNAL(tabLeftClicked(int)),this,SLOT(tabLeftClick(int)));
}

QTabBar* QSpecTabWidget::tabBar() const
{
	return QTabWidget::tabBar();
}

void QSpecTabWidget::tabLeftClick(int index)
{
    emit tabLeftClicked(index);
}

void QSpecTabWidget::tabRightClick(int index)
{
    emit tabRightClicked(index);
    if (mainTabWidget) {
        CSnippetViewer * tb = qobject_cast<CSnippetViewer *>(widget(index));
        if (tb!=NULL) tb->closeTab();
        CSearchTab * tbb = qobject_cast<CSearchTab *>(widget(index));
        if (tbb!=NULL) tbb->closeTab();
    }
}

void QSpecTabWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (mainTabWidget && event->button()==Qt::LeftButton) createTab();
}

bool QSpecTabWidget::event(QEvent *event)
{
    if (event->type()==QEvent::ToolTip) {
        QHelpEvent* e = dynamic_cast<QHelpEvent *>(event);
        QPoint gpos = QPoint();
        QPoint lpos = QPoint();
        if (e!=NULL) {
            gpos = e->globalPos();
            lpos = e->pos();
        }
        emit tooltipRequested(gpos,lpos);
        event->accept();
        return true;
    } else
        return QTabWidget::event(event);
}

void QSpecTabWidget::createTab()
{
    if (mainTabWidget && parentWnd!=NULL)
        parentWnd->openEmptyBrowser();
}

void QSpecTabWidget::selectNextTab()
{
    if (count()==0) return;
    if (currentIndex()==(count()-1))
        setCurrentIndex(0);
    else
        setCurrentIndex(currentIndex()+1);
}

void QSpecTabWidget::selectPrevTab()
{
    if (count()==0) return;
    if (currentIndex()==0)
        setCurrentIndex((count()-1));
    else
        setCurrentIndex(currentIndex()-1);
}

QSpecTabBar::QSpecTabBar(QWidget *p)
	: QTabBar(p)
{

}

void QSpecTabBar::mousePressEvent(QMouseEvent *event)
{
    Qt::MouseButton btn = event->button();
    QPoint pos = event->pos();
    int tabCount = count();
    for (int i=0;i<tabCount;i++) {
        if (event->button()==Qt::RightButton) {
            if (tabRect(i).contains(event->pos())) {
                emit tabRightClicked(i);
                return;
            }
        } else if (event->button()==Qt::LeftButton) {
            if (tabRect(i).contains(event->pos())) {
                emit tabLeftClicked(i);
            }
        }
    }
    QTabBar::mousePressEvent(event);
    tabCount = count();
    for (int i=0;i<tabCount;i++) {
        if (btn==Qt::RightButton) {
            if (tabRect(i).contains(pos)) {
                emit tabRightPostClicked(i);
            }
        } else if (btn==Qt::LeftButton) {
            if (tabRect(i).contains(pos)) {
                emit tabLeftPostClicked(i);
            }
        }
    }
}

QSpecWebView::QSpecWebView(QWidget *parent, CMainWindow* mainWnd)
    : QWebView(parent)
{
    parentWnd = mainWnd;
}

QWebView* QSpecWebView::createWindow(QWebPage::WebWindowType)
{
    CSnippetViewer* sv = new CSnippetViewer(parentWnd);
    return sv->txtBrowser;
}

QSpecCookieJar::QSpecCookieJar(QWidget *parent)
    : QNetworkCookieJar(parent)
{

}

void QSpecCookieJar::loadCookies(QByteArray* dump)
{
    QDataStream sv(dump,QIODevice::ReadOnly);
    int cnt;
    sv >> cnt;
    QList<QNetworkCookie> cookies;
    cookies.clear();
    for (int i=0;i<cnt;i++) {
        QByteArray ckvalue;
        sv >> ckvalue;
        QList<QNetworkCookie> cl = QNetworkCookie::parseCookies(ckvalue);
        if (cl.count()>0) cookies.append(cl);
    }
    setAllCookies(cookies);
}

QByteArray QSpecCookieJar::saveCookies()
{
    QByteArray dump;
    dump.clear();
    QDataStream sv(&dump,QIODevice::WriteOnly);
    QList<QNetworkCookie> cookies = allCookies();
    int cnt = cookies.count();
    sv << cnt;
    for (int i=0;i<cnt;i++) {
        if (!cookies[i].isSessionCookie()) {
            if (cookies[i].expirationDate()>=QDateTime::currentDateTime()) {
                sv << cookies[i].toRawForm(QNetworkCookie::Full);
            }
        }
    }
    return dump;
}

void QSpecCookieJar::clearCookies()
{
    QList<QNetworkCookie> cookies;
    cookies.clear();
    allCookies().clear();
    setAllCookies(cookies);
}

int QSpecMenuStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                               QStyleHintReturn *returnData) const
{
    if ( hint == SH_DrawMenuBarSeparator)
        return true;
    else
        return QProxyStyle::styleHint(hint, option, widget, returnData);
}

bool QSpecWebPage::acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request, NavigationType type)
{
    if (type == NavigationTypeLinkClicked) {
        emit linkClickedExt(frame,request.url());
        return false;
    }
    return true;
}

QNetworkReply* QSpecNetworkAccessManager::createRequest(Operation op, const QNetworkRequest &req,
                                              QIODevice *outgoingData)
{
    if (gSet->debugNetReqLogging)
        qDebug() << req.url();

    if (gSet->isUrlBlocked(req.url())) {
        qDebug() << "adblock - skipping" << req.url();
        return QNetworkAccessManager::createRequest(QNetworkAccessManager::GetOperation,
                                                    QNetworkRequest(QUrl()));
    } else {
        QNetworkRequest rq(req);
        if (!req.hasRawHeader("Referer")) {
            cachePolicy.insert(req.url(),req.attribute(QNetworkRequest::CacheLoadControlAttribute,
                                                       QNetworkRequest::PreferNetwork));
        }
        else {
            QUrl ref(QString::fromLatin1(req.rawHeader("Referer")));
            if (!ref.isEmpty() && cachePolicy.contains(ref)) {
                rq.setAttribute(QNetworkRequest::CacheLoadControlAttribute,cachePolicy[ref]);
            }
        }
        return QNetworkAccessManager::createRequest(op, rq, outgoingData);
    }
}


void QSpecToolTipLabel::hideEvent(QHideEvent *)
{
    emit labelHide();
}

QSpecToolTipLabel::QSpecToolTipLabel(const QString &text)
    : QLabel()
{
    setText(text);
}


QHotkeyEdit::QHotkeyEdit(QWidget *parent) :
    QLineEdit(parent)
{
    p_shortcut = QKeySequence();
    setReadOnly(true);
}

QKeySequence QHotkeyEdit::keySequence() const
{
    return p_shortcut;
}

void QHotkeyEdit::setKeySequence(const QKeySequence &sequence)
{
    p_shortcut = sequence;
    updateSequenceView();
}

void QHotkeyEdit::keyPressEvent(QKeyEvent *event)
{
    int seq = 0;
    switch ( event->modifiers())
    {
        case Qt::ShiftModifier : seq = Qt::SHIFT; break;
        case Qt::ControlModifier : seq = Qt::CTRL; break;
        case Qt::AltModifier : seq = Qt::ALT; break;
        case Qt::MetaModifier : seq = Qt::META; break;
    }
    if ((event->key()!=0) && (event->key()!=Qt::Key_unknown))
        setKeySequence(seq+event->key());
    event->accept();
}

void QHotkeyEdit::updateSequenceView()
{
    setText(p_shortcut.toString());
}
