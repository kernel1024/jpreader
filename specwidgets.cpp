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
    m_tabBar = new QSpecTabBar(this);
    setTabBar(m_tabBar);
    connect(m_tabBar,SIGNAL(tabRightClicked(int)),this,SLOT(tabRightClick(int)));
    connect(m_tabBar,SIGNAL(tabLeftClicked(int)),this,SLOT(tabLeftClick(int)));
}

QSpecTabBar *QSpecTabWidget::tabBar() const
{
    return m_tabBar;
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
    : QTabBar(p), m_browserTabs(false)
{

}

void QSpecTabBar::setBrowserTabs(bool enabled)
{
    m_browserTabs = enabled;
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

QSize QSpecTabBar::minimumTabSizeHint(int index) const
{
    if (!m_browserTabs || count()==0)
        return QTabBar::minimumTabSizeHint(index);

    QSize sz = QTabBar::minimumTabSizeHint(index);
    if (sz.width()>(width()/count()))
        sz.setWidth(width()/count());
    return sz;
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
        tabWidget->tabBar()->setTabButton(i,QTabBar::RightSide,b);
    }
    tabTitle = getDocTitle();
    if (setFocused) tabWidget->setCurrentWidget(this);
    parentWnd->updateTitle();
    parentWnd->updateTabs();
}

void QSpecTabContainer::updateTabIcon(const QIcon &icon)
{
    if (!gSet->showFavicons) return;
    if (tabWidget==NULL) return;
    tabWidget->setTabIcon(tabWidget->indexOf(this),icon);
}

void QSpecTabContainer::detachTab()
{
    if (tabWidget->count()<=1) return;

    QSpecTabContainer* ntab = NULL;

    CSnippetViewer* snv = qobject_cast<CSnippetViewer *>(this);
    if (snv!=NULL) {
        QString url = "about://blank";
        if (!snv->fileChanged) url=snv->urlEdit->text();

        CMainWindow* mwnd = gSet->addMainWindow(false,false);
        CSnippetViewer* sv = new CSnippetViewer(mwnd, url);
        ntab = sv;

        if (snv->fileChanged || url.isEmpty()) {
            snv->txtBrowser->page()->toHtml([sv,snv,this](const QString& html) {
                if (!html.isEmpty())
                    sv->txtBrowser->setHtml(html,snv->txtBrowser->page()->url());
            });
        }
    }

    CSearchTab* stb = qobject_cast<CSearchTab *>(this);
    if (stb!=NULL) {
        QString query = stb->getLastQuery();
        if (!query.isEmpty()) {
            CMainWindow* mwnd = gSet->addMainWindow(false,false);
            CSearchTab* st = new CSearchTab(mwnd);
            ntab = st;
            st->searchTerm(query,false);
        }
    }

    if (ntab!=NULL) {
        closeTab();
        ntab->activateWindow();
        ntab->parentWnd->raise();
    }

/*  failed with Qt5 openGL backend...
 *  QSGTextureAtlas: texture atlas allocation failed, code=501

    tabWidget->removeTab(tabWidget->indexOf(this));
    parentWnd = mwnd;
    setParent(mwnd);
    bindToTab(mwnd->tabMain);
    activateWindow();
    mwnd->raise();*/
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
    emit linkClickedExt(url,(int)type,isMainFrame);

    if (gSet->debugNetReqLogging) {
        if (isMainFrame)
            qInfo() << "Net request (main frame):" << url;
        else
            qInfo() << "Net request (sub frame):" << url;
    }

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

#ifdef WEBENGINE_56

QSpecUrlInterceptor::QSpecUrlInterceptor(QObject *p)
    : QWebEngineUrlRequestInterceptor(p)
{

}

bool QSpecUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (gSet->isUrlBlocked(info.requestUrl())) {
        if (gSet->debugNetReqLogging)
            qWarning() << "Net request:" << info.requestUrl() << "BLOCKED";

        info.block(true);

        return true;
    }

    if (gSet->debugNetReqLogging)
        qInfo() << "Net request:" << info.requestUrl();

    return false;
}

#endif

QSpecUrlHistoryModel::QSpecUrlHistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{

}

int QSpecUrlHistoryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return gSet->mainHistory.count();
}

QVariant QSpecUrlHistoryModel::data(const QModelIndex &index, int role) const
{
    int idx = index.row();

    if (idx<0 || idx>=gSet->mainHistory.count()) return QVariant();

    if (role==Qt::DisplayRole || role==Qt::EditRole)
        return gSet->mainHistory.at(idx).url.toString();

    return QVariant();
}

Qt::ItemFlags QSpecUrlHistoryModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}
