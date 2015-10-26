#include <QTextCharFormat>
#include <QRegExp>
#include <QUrl>
#include <QUrlQuery>
#include <QMessageLogger>
#include <vector>
#include "goldendictmgr.h"
#include "specwidgets.h"
#include "snviewer.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "searchtab.h"
#include "genericfuncs.h"
#include "dictionary.hh"

CSpecTabWidget::CSpecTabWidget(QWidget *p)
	: QTabWidget(p)
{
    parentWnd = NULL;
    mainTabWidget = true;
    m_tabBar = new CSpecTabBar(this);
    setTabBar(m_tabBar);
    connect(m_tabBar,SIGNAL(tabRightClicked(int)),this,SLOT(tabRightClick(int)));
    connect(m_tabBar,SIGNAL(tabLeftClicked(int)),this,SLOT(tabLeftClick(int)));
}

CSpecTabBar *CSpecTabWidget::tabBar() const
{
    return m_tabBar;
}

void CSpecTabWidget::tabLeftClick(int index)
{
    emit tabLeftClicked(index);
}

void CSpecTabWidget::tabRightClick(int index)
{
    emit tabRightClicked(index);
    if (mainTabWidget) {
        CSpecTabContainer * tb = qobject_cast<CSpecTabContainer *>(widget(index));
        if (tb!=NULL) tb->closeTab();
    }
}

void CSpecTabWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (mainTabWidget && event->button()==Qt::LeftButton) createTab();
}

bool CSpecTabWidget::event(QEvent *event)
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

void CSpecTabWidget::createTab()
{
    if (mainTabWidget && parentWnd!=NULL)
        parentWnd->openEmptyBrowser();
}

void CSpecTabWidget::selectNextTab()
{
    if (count()==0) return;
    if (currentIndex()==(count()-1))
        setCurrentIndex(0);
    else
        setCurrentIndex(currentIndex()+1);
}

void CSpecTabWidget::selectPrevTab()
{
    if (count()==0) return;
    if (currentIndex()==0)
        setCurrentIndex((count()-1));
    else
        setCurrentIndex(currentIndex()-1);
}

CSpecTabBar::CSpecTabBar(QWidget *p)
    : QTabBar(p), m_browserTabs(false)
{

}

void CSpecTabBar::setBrowserTabs(bool enabled)
{
    m_browserTabs = enabled;
}

void CSpecTabBar::mousePressEvent(QMouseEvent *event)
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

QSize CSpecTabBar::minimumTabSizeHint(int index) const
{
    if (!m_browserTabs || count()==0)
        return QTabBar::minimumTabSizeHint(index);

    QSize sz = QTabBar::minimumTabSizeHint(index);
    if (sz.width()>(width()/count()))
        sz.setWidth(width()/count());
    return sz;
}

int CSpecMenuStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                               QStyleHintReturn *returnData) const
{
    if ( hint == SH_DrawMenuBarSeparator)
        return true;
    else
        return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void CSpecToolTipLabel::hideEvent(QHideEvent *)
{
    emit labelHide();
}

CSpecToolTipLabel::CSpecToolTipLabel(const QString &text)
    : QLabel()
{
    setText(text);
}


CHotkeyEdit::CHotkeyEdit(QWidget *parent) :
    QLineEdit(parent)
{
    p_shortcut = QKeySequence();
    setReadOnly(true);
}

QKeySequence CHotkeyEdit::keySequence() const
{
    return p_shortcut;
}

void CHotkeyEdit::setKeySequence(const QKeySequence &sequence)
{
    p_shortcut = sequence;
    updateSequenceView();
}

void CHotkeyEdit::keyPressEvent(QKeyEvent *event)
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

void CHotkeyEdit::updateSequenceView()
{
    setText(p_shortcut.toString());
}


CSpecTabContainer::CSpecTabContainer(CMainWindow *parent)
    : QWidget(parent)
{
    parentWnd = parent;
    tabWidget=NULL;
}

void CSpecTabContainer::bindToTab(CSpecTabWidget *tabs, bool setFocused)
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

void CSpecTabContainer::updateTabIcon(const QIcon &icon)
{
    if (!gSet->showFavicons) return;
    if (tabWidget==NULL) return;
    tabWidget->setTabIcon(tabWidget->indexOf(this),icon);
}

void CSpecTabContainer::detachTab()
{
    if (tabWidget->count()<=1) return;

    CSpecTabContainer* ntab = NULL;

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

void CSpecTabContainer::closeTab(bool nowait)
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


CSpecWebPage::CSpecWebPage(CSnippetViewer *parent)
    : QWebEnginePage(parent)
{
    parentViewer = parent;
}

CSpecWebPage::CSpecWebPage(QWebEngineProfile *profile, CSnippetViewer *parent)
    : QWebEnginePage(profile, parent)
{
    parentViewer = parent;
}

bool CSpecWebPage::acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame)
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

bool CSpecWebPage::certificateError(const QWebEngineCertificateError &certificateError)
{
    Q_UNUSED(certificateError);

    return gSet->ignoreSSLErrors;
}

QWebEnginePage *CSpecWebPage::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type);

    if (parentViewer==NULL) return NULL;

    CSnippetViewer* sv = new CSnippetViewer(parentViewer->parentWnd);
    return sv->txtBrowser->page();
}

void CSpecWebPage::javaScriptConsoleMessage(QWebEnginePage::JavaScriptConsoleMessageLevel level,
                                            const QString &message, int lineNumber,
                                            const QString &sourceID)
{
    if (!gSet->jsLogConsole) return;

    QByteArray ssrc = sourceID.toUtf8();
    if (ssrc.isEmpty())
        ssrc = QByteArray("global");
    const char* src = ssrc.constData();
    QByteArray smsg = message.toUtf8();
    const char* msg = smsg.constData();

    switch (level) {
        case QWebEnginePage::InfoMessageLevel:
            QMessageLogger(src, lineNumber, NULL, "JavaScript").info() << msg;
            break;
        case QWebEnginePage::WarningMessageLevel:
            QMessageLogger(src, lineNumber, NULL, "JavaScript").warning() << msg;
            break;
        case QWebEnginePage::ErrorMessageLevel:
            QMessageLogger(src, lineNumber, NULL, "JavaScript").critical() << msg;
            break;
        default:
            QMessageLogger(src, lineNumber, NULL, "JavaScript").debug() << msg;
            break;
    }
}

CSpecLogHighlighter::CSpecLogHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{

}

void CSpecLogHighlighter::highlightBlock(const QString &text)
{
    formatBlock(text,QRegExp("^\\S{,8}",Qt::CaseInsensitive),Qt::black,true);
    formatBlock(text,QRegExp("\\s(\\S+\\s)?Debug:\\s",Qt::CaseInsensitive),Qt::black,true);
    formatBlock(text,QRegExp("\\s(\\S+\\s)?Warning:\\s",Qt::CaseInsensitive),Qt::darkRed,true);
    formatBlock(text,QRegExp("\\s(\\S+\\s)?Critical:\\s",Qt::CaseInsensitive),Qt::red,true);
    formatBlock(text,QRegExp("\\s(\\S+\\s)?Fatal:\\s",Qt::CaseInsensitive),Qt::red,true);
    formatBlock(text,QRegExp("\\s(\\S+\\s)?Info:\\s",Qt::CaseInsensitive),Qt::darkBlue,true);
    formatBlock(text,QRegExp("\\sBLOCKED\\s",Qt::CaseSensitive),Qt::darkRed,true);
    formatBlock(text,QRegExp("\\(\\S+\\)$",Qt::CaseInsensitive),Qt::gray,false,true);
}

void CSpecLogHighlighter::formatBlock(const QString &text, const QRegExp &exp,
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

CSpecUrlInterceptor::CSpecUrlInterceptor(QObject *p)
    : QWebEngineUrlRequestInterceptor(p)
{

}

bool CSpecUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    QString rule;
    if (gSet->isUrlBlocked(info.requestUrl(),rule)) {
        if (gSet->debugNetReqLogging)
            qWarning() << "Net request:" << info.requestUrl() << "BLOCKED" <<
                          QString("(rule: '%1')").arg(rule);

        info.block(true);

        return true;
    }

    if (gSet->debugNetReqLogging)
        qInfo() << "Net request:" << info.requestUrl();

    return false;
}

CSpecGDSchemeHandler::CSpecGDSchemeHandler(QObject *parent)
    : QWebEngineUrlSchemeHandler(parent)
{

}

void CSpecGDSchemeHandler::requestStarted(QWebEngineUrlRequestJob *request)
{
    if (request==NULL) return;

    // request->fail doesn't work as expected...

    QByteArray rplb;
    rplb.clear();

    if (request->requestUrl().scheme().toLower()!="gdlookup") {
        rplb = makeSimpleHtml(tr("Error"),
                              tr("Scheme '%1' not supported for 'gdlookup' scheme handler.")
                              .arg(request->requestUrl().scheme())).toUtf8();
        QIODevice *reply = new CMemFile(rplb);
        request->reply("text/html",reply);
        return;
    }

    CIOEventLoop ev;
    QString mime;
    sptr<Dictionary::DataRequest> dr = gSet->dictNetMan->getResource(request->requestUrl(),mime);

    connect(dr.get(),SIGNAL(finished()),&ev,SLOT(finished()));
    connect(request,SIGNAL(destroyed(QObject*)),&ev,SLOT(objDestroyed(QObject*)));
    QTimer::singleShot(15000,&ev,SLOT(timeout()));

    int ret = ev.exec();

    if (ret==2) { // Request destroyed
        return;

    } else if (ret==1) { // Timeout
        rplb = makeSimpleHtml(tr("Error"),
                              tr("Dictionary request timeout for query '%1'.")
                              .arg(request->requestUrl().toString())).toUtf8();
        QIODevice *reply = new CMemFile(rplb);
        request->reply("text/html",reply);

    } else if (dr->isFinished() && dr->dataSize()>0 && ret==0) { // Dictionary success
        std::vector<char> vc = dr->getFullData();
        QByteArray res = QByteArray(reinterpret_cast<const char*>(vc.data()), vc.size());
        QIODevice *reply = new CMemFile(res);
        request->reply(mime.toLatin1(),reply);

    } else { // Dictionary error
        rplb = makeSimpleHtml(tr("Error"),
                              tr("Dictionary request failed for query '%1'.<br/>Error: %2.")
                              .arg(request->requestUrl().toString())
                              .arg(dr->getErrorString())).toUtf8();
        QIODevice *reply = new CMemFile(rplb);
        request->reply("text/html",reply);
    }
}

#endif

CSpecUrlHistoryModel::CSpecUrlHistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{

}

int CSpecUrlHistoryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return gSet->mainHistory.count();
}

QVariant CSpecUrlHistoryModel::data(const QModelIndex &index, int role) const
{
    int idx = index.row();

    if (idx<0 || idx>=gSet->mainHistory.count()) return QVariant();

    if (role==Qt::DisplayRole || role==Qt::EditRole)
        return gSet->mainHistory.at(idx).url.toString();

    return QVariant();
}

Qt::ItemFlags CSpecUrlHistoryModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

CMemFile::CMemFile(const QByteArray &fileData)
    : data(fileData), origLen(fileData.length())
{
    setOpenMode(QIODevice::ReadOnly);

    QTimer::singleShot(0, this, &QIODevice::readyRead);
    QTimer::singleShot(0, this, &QIODevice::readChannelFinished);
}

qint64 CMemFile::bytesAvailable() const
{
    return data.length() + QIODevice::bytesAvailable();
}

void CMemFile::close()
{
    QIODevice::close();
    deleteLater();
}

qint64 CMemFile::readData(char *buffer, qint64 maxlen)
{
    qint64 len = qMin(qint64(data.length()), maxlen);
    if (len) {
        memcpy(buffer, data.constData(), len);
        data.remove(0, len);
    }
    return len;
}

qint64 CMemFile::writeData(const char *buffer, qint64 maxlen)
{
    Q_UNUSED(buffer);
    Q_UNUSED(maxlen);

    return 0;
}

CIOEventLoop::CIOEventLoop(QObject *parent)
    : QEventLoop(parent)
{

}

void CIOEventLoop::finished()
{
    exit(0);
}

void CIOEventLoop::timeout()
{
    exit(1);
}

void CIOEventLoop::objDestroyed(QObject *obj)
{
    Q_UNUSED(obj);

    exit(2);
}
