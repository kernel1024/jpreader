#include <QTextCharFormat>
#include <QRegExp>
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
        QSpecTabContainer * tb = qobject_cast<QSpecTabContainer *>(widget(index));
        if (tb!=NULL) tb->closeTab();
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

int QSpecMenuStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                               QStyleHintReturn *returnData) const
{
    if ( hint == SH_DrawMenuBarSeparator)
        return true;
    else
        return QProxyStyle::styleHint(hint, option, widget, returnData);
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


QSpecTabContainer::QSpecTabContainer(CMainWindow *parent)
    : QWidget(parent)
{
    parentWnd = parent;
    tabWidget=NULL;
}

void QSpecTabContainer::bindToTab(QSpecTabWidget *tabs, bool setFocused)
{
    tabWidget=tabs;
    if (tabWidget==NULL) return;
    int i = tabWidget->addTab(this,getDocTitle());
    if (gSet->showTabCloseButtons) {
        QPushButton* b = new QPushButton(QIcon::fromTheme("dialog-close"),"");
        b->setFlat(true);
        int sz = tabWidget->tabBar()->fontMetrics().height();
        b->resize(QSize(sz,sz));
        connect(b,SIGNAL(clicked()),this,SLOT(closeTab()));
        tabWidget->tabBar()->setTabButton(i,QTabBar::LeftSide,b);
    }
    tabTitle = getDocTitle();
    if (setFocused) tabWidget->setCurrentWidget(this);
    parentWnd->updateTitle();
    parentWnd->updateTabs();
}

void QSpecTabContainer::detachTab()
{
    if (tabWidget->count()<=1) return;

    CMainWindow* mwnd = gSet->addMainWindow(false,false);

    tabWidget->removeTab(tabWidget->indexOf(this));
    parentWnd = mwnd;
    setParent(mwnd);
    bindToTab(mwnd->tabMain);
    activateWindow();
    mwnd->raise();
}

void QSpecTabContainer::closeTab(bool nowait)
{
    if (!canClose()) return;
    if (tabWidget->count()<=1) return; // prevent closing while only 1 tab remains

    if (!nowait) {
        if (gSet->blockTabCloseActive) return;
        gSet->blockTabClose();
    }
    if (tabWidget!=NULL) {
        if (parentWnd->lastTabIdx>=0) tabWidget->setCurrentIndex(parentWnd->lastTabIdx);
        tabWidget->removeTab(tabWidget->indexOf(this));
    }
    recycleTab();
    parentWnd->updateTitle();
    parentWnd->updateTabs();
    parentWnd->checkTabs();
    deleteLater();
}


QSpecWebPage::QSpecWebPage(CSnippetViewer *parent)
    : QWebEnginePage(parent)
{
    parentViewer = parent;
}

QSpecWebPage::QSpecWebPage(QWebEngineProfile *profile, CSnippetViewer *parent)
    : QWebEnginePage(profile, parent)
{
    parentViewer = parent;
}

bool QSpecWebPage::acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame)
{
    Q_UNUSED(type);
    Q_UNUSED(isMainFrame);

    emit linkClickedExt(url,(int)type);

    return !gSet->isUrlBlocked(url);
}

bool QSpecWebPage::certificateError(const QWebEngineCertificateError &certificateError)
{
    Q_UNUSED(certificateError);

    return gSet->ignoreSSLErrors;
}

QWebEnginePage *QSpecWebPage::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type);

    if (parentViewer==NULL) return NULL;

    CSnippetViewer* sv = new CSnippetViewer(parentViewer->parentWnd,QUrl(),QStringList(),false);
    return sv->txtBrowser->page();
}

QSpecLogHighlighter::QSpecLogHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{

}

void QSpecLogHighlighter::highlightBlock(const QString &text)
{
    formatBlock(text,QRegExp("^\\S{,8}",Qt::CaseInsensitive),Qt::black,true);
    formatBlock(text,QRegExp("\\sDebug:\\s",Qt::CaseInsensitive),Qt::black,true);
    formatBlock(text,QRegExp("\\sWarning:\\s",Qt::CaseInsensitive),Qt::darkRed,true);
    formatBlock(text,QRegExp("\\sCritical:\\s",Qt::CaseInsensitive),Qt::red,true);
    formatBlock(text,QRegExp("\\sFatal:\\s",Qt::CaseInsensitive),Qt::red,true);
    formatBlock(text,QRegExp("\\sInfo:\\s",Qt::CaseInsensitive),Qt::darkBlue,true);
    formatBlock(text,QRegExp("\\(\\S+\\)$",Qt::CaseInsensitive),Qt::gray,false,true);
}

void QSpecLogHighlighter::formatBlock(const QString &text, const QRegExp &exp,
                                      const QColor &color,
                                      bool weight,
                                      bool italic,
                                      bool underline,
                                      bool strikeout)
{
    if (text.isEmpty()) return;

    QTextCharFormat fmt;
    fmt.setForeground(color);
    if (weight)
        fmt.setFontWeight(QFont::Bold);
    else
        fmt.setFontWeight(QFont::Normal);
    fmt.setFontItalic(italic);
    fmt.setFontUnderline(underline);
    fmt.setFontStrikeOut(strikeout);

    int pos = 0;
    while ((pos=exp.indexIn(text,pos)) != -1) {
        int length = exp.matchedLength();
        setFormat(pos, length, fmt);
        pos += length;
    }
}
