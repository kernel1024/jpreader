#include <QTextCharFormat>
#include <QRegExp>
#include <QUrl>
#include <QUrlQuery>
#include <QMessageLogger>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineCertificateError>
#include <vector>
#include <goldendictlib/goldendictmgr.hh>
#include <goldendictlib/dictionary.hh>
#include "specwidgets.h"
#include "snviewer.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "searchtab.h"
#include "genericfuncs.h"
#include "userscript.h"
#include "snctxhandler.h"

CSpecTabWidget::CSpecTabWidget(QWidget *p)
	: QTabWidget(p)
{
    parentWnd = nullptr;
    mainTabWidget = true;
    m_tabBar = new CSpecTabBar(this);
    setTabBar(m_tabBar);
    connect(m_tabBar, &CSpecTabBar::tabRightClicked,this,&CSpecTabWidget::tabRightClick);
    connect(m_tabBar, &CSpecTabBar::tabLeftClicked,this,&CSpecTabWidget::tabLeftClick);
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
        auto tb = qobject_cast<CSpecTabContainer *>(widget(index));
        if (tb && tb->parentWnd) {
            if (!tb->parentWnd->titleRenamedLock.isActive())
                tb->closeTab();
        }
    }
}

void CSpecTabWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (mainTabWidget && event->button()==Qt::LeftButton) createTab();
}

bool CSpecTabWidget::event(QEvent *event)
{
    if (event->type()==QEvent::ToolTip) {
        auto e = dynamic_cast<QHelpEvent *>(event);
        QPoint gpos = QPoint();
        QPoint lpos = QPoint();
        if (e) {
            gpos = e->globalPos();
            lpos = e->pos();
        }
        emit tooltipRequested(gpos,lpos);
        event->accept();
        return true;
    }

    return QTabWidget::event(event);
}

void CSpecTabWidget::createTab()
{
    if (mainTabWidget && parentWnd)
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

CSpecTabBar::CSpecTabBar(CSpecTabWidget *p)
    : QTabBar(p), m_browserTabs(false)
{
    m_tabWidget = p;
    m_dragStart = QPoint(0,0);
    m_draggingTab = nullptr;
}

CSpecTabBar::CSpecTabBar(QWidget *p)
    : QTabBar(p), m_browserTabs(false)
{
    m_tabWidget = nullptr;
    m_dragStart = QPoint(0,0);
    m_draggingTab = nullptr;
}

void CSpecTabBar::setBrowserTabs(bool enabled)
{
    m_browserTabs = enabled;
}

void CSpecTabBar::mousePressEvent(QMouseEvent *event)
{
    m_draggingTab = nullptr;
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
                if (m_tabWidget) {
                    m_draggingTab = qobject_cast<CSpecTabContainer *>(m_tabWidget->widget(i));
                    m_dragStart = event->pos();
                }
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

void CSpecTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_draggingTab = nullptr;
    QTabBar::mouseReleaseEvent(event);
}

void CSpecTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if ((m_draggingTab) &&
            (m_tabWidget) &&
            (event->buttons() == Qt::LeftButton)) {

        QRect wr = QRect(mapToGlobal(QPoint(0,0)),size()).marginsAdded(QMargins(50,50,50,100));

        if (!wr.contains(event->globalPos())) {

            // finish up local drag inside QTabBar
            QMouseEvent evr(QEvent::MouseButtonRelease, m_dragStart, Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier);
            QTabBar::mouseReleaseEvent(&evr);

            m_draggingTab->outsideDragStart();

            m_draggingTab = nullptr;
        }
    }
    QTabBar::mouseMoveEvent(event);
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
        return static_cast<int>(true);

    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void CSpecToolTipLabel::hideEvent(QHideEvent *)
{
    emit labelHide();
}

CSpecToolTipLabel::CSpecToolTipLabel(const QString &text)
{
    setText(text);
}

CSpecTabContainer::CSpecTabContainer(CMainWindow *parent)
    : QWidget(parent)
{
    parentWnd = parent;
    tabWidget=nullptr;
}

void CSpecTabContainer::bindToTab(CSpecTabWidget *tabs, bool setFocused)
{
    tabWidget=tabs;
    if (tabWidget==nullptr) return;
    int i = tabWidget->addTab(this,getDocTitle());
    if (gSet->settings.showTabCloseButtons) {
        QPushButton* b = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-close")),QString());
        b->setFlat(true);
        int sz = tabWidget->tabBar()->fontMetrics().height();
        b->resize(QSize(sz,sz));
        connect(b, &QPushButton::clicked,this,&CSpecTabContainer::closeTab);
        tabWidget->tabBar()->setTabButton(i,QTabBar::RightSide,b);
    }
    tabTitle = getDocTitle();
    if (setFocused) tabWidget->setCurrentWidget(this);
    parentWnd->updateTitle();
    parentWnd->updateTabs();
}

void CSpecTabContainer::updateTabIcon(const QIcon &icon)
{
    if (!gSet->webProfile->settings()->
            testAttribute(QWebEngineSettings::AutoLoadIconsForPage)) return;
    if (tabWidget==nullptr) return;
    tabWidget->setTabIcon(tabWidget->indexOf(this),icon);
}

void CSpecTabContainer::detachTab()
{
    if (tabWidget->count()<=1) return;

/*  Classic method - create new tab and copy contents
 *
    CSpecTabContainer* ntab = nullptr;

    CSnippetViewer* snv = qobject_cast<CSnippetViewer *>(this);
    if (snv) {
        QString url("about://blank");
        if (!snv->fileChanged) url=snv->urlEdit->text();

        CMainWindow* mwnd = gSet->ui.addMainWindow(false,false);
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
    if (stb) {
        QString query = stb->getLastQuery();
        if (!query.isEmpty()) {
            CMainWindow* mwnd = gSet->ui.addMainWindow(false,false);
            CSearchTab* st = new CSearchTab(mwnd);
            ntab = st;
            st->searchTerm(query,false);
        }
    }

    if (ntab) {
        closeTab();
        ntab->activateWindow();
        ntab->parentWnd->raise();
    } */

/*
 *  Reparenting
 *  Can caught failure with Qt5 openGL backend...
 *  QSGTextureAtlas: texture atlas allocation failed, code=501 */

    CMainWindow* mwnd = gSet->ui.addMainWindowEx(false,false);
    tabWidget->removeTab(tabWidget->indexOf(this));
    parentWnd = mwnd;
    setParent(mwnd);
    bindToTab(mwnd->tabMain);
    activateWindow();
    mwnd->raise();
}

void CSpecTabContainer::closeTab(bool nowait)
{
    if (!canClose()) return;
    if (tabWidget->count()<=1) return; // prevent closing while only 1 tab remains

    if (!nowait) {
        if (gSet->blockTabCloseActive) return;
        gSet->blockTabClose();
    }
    if (tabWidget) {
        if ((parentWnd->lastTabIdx>=0) &&
                (parentWnd->lastTabIdx<tabWidget->count())) {
            tabWidget->setCurrentIndex(parentWnd->lastTabIdx);
        }
        tabWidget->removeTab(tabWidget->indexOf(this));
    }
    recycleTab();
    parentWnd->updateTitle();
    parentWnd->updateTabs();
    parentWnd->checkTabs();
    deleteLater();
}

CSpecWebView::CSpecWebView(QWidget *parent)
    : QWebEngineView(parent)
{
    parentViewer = qobject_cast<CSnippetViewer *>(parent);
    if (parentViewer==nullptr)
        qCritical() << "parentViewer is nullptr";

    m_page = new CSpecWebPage(gSet->webProfile, this);
    setPage(m_page);
}

CSpecWebView::CSpecWebView(CSnippetViewer *parent)
    : QWebEngineView(parent)
{
    parentViewer = parent;
    m_page = new CSpecWebPage(gSet->webProfile, this);
    setPage(m_page);
}

CSpecWebPage *CSpecWebView::customPage() const
{
    return m_page;
}

QWebEngineView *CSpecWebView::createWindow(QWebEnginePage::WebWindowType type)
{
    if (parentViewer==nullptr) return nullptr;

    CSnippetViewer* sv = new CSnippetViewer(parentViewer->parentWnd,QUrl(),QStringList(),
                                            (type!=QWebEnginePage::WebBrowserBackgroundTab));
    return sv->txtBrowser;
}

void CSpecWebView::contextMenuEvent(QContextMenuEvent *event)
{
    if (parentViewer)
        parentViewer->ctxHandler->contextMenu(event->pos(), m_page->contextMenuData());
}

CSpecWebPage::CSpecWebPage(QObject *parent)
    : QWebEnginePage(parent)
{

}

CSpecWebPage::CSpecWebPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{

}

bool CSpecWebPage::acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type,
                                           bool isMainFrame)
{
    emit linkClickedExt(url,static_cast<int>(type),isMainFrame);

    if (gSet->settings.debugNetReqLogging) {
        if (isMainFrame)
            qInfo() << "Net request (main frame):" << url;
        else
            qInfo() << "Net request (sub frame):" << url;
    }

    bool blocked = gSet->isUrlBlocked(url);

    if (!blocked) {
        // NoScript discovery reload
        gSet->clearNoScriptPageHosts(url.toString(CSettings::adblockUrlFmt));

        // Userscripts
        const QVector<CUserScript> sl(gSet->getUserScriptsForUrl(url, isMainFrame, false));

        if (!sl.isEmpty())
            scripts().clear();

        for (int i = 0; i < sl.count(); ++i)
        {
            QWebEngineScript::InjectionPoint injectionPoint(QWebEngineScript::DocumentReady);

            if (sl.at(i).getInjectionTime() == CUserScript::DocumentCreationTime)
                injectionPoint = QWebEngineScript::DocumentCreation;
            else if (sl.at(i).getInjectionTime() == CUserScript::DeferredTime)
                injectionPoint = QWebEngineScript::Deferred;

            QWebEngineScript script;
            script.setSourceCode(sl.at(i).getSource());
            script.setRunsOnSubFrames(sl.at(i).shouldRunOnSubFrames());
            script.setInjectionPoint(injectionPoint);

            scripts().insert(script);
        }
    }

    return !blocked;
}

bool CSpecWebPage::certificateError(const QWebEngineCertificateError &certificateError)
{
    qWarning() << "SSL certificate error" << certificateError.error()
               << certificateError.errorDescription() << certificateError.url();

    return gSet->settings.ignoreSSLErrors;
}

void CSpecWebPage::javaScriptConsoleMessage(QWebEnginePage::JavaScriptConsoleMessageLevel level,
                                            const QString &message, int lineNumber,
                                            const QString &sourceID)
{
    if (!gSet->settings.jsLogConsole) return;

    QByteArray ssrc = sourceID.toUtf8();
    if (ssrc.isEmpty())
        ssrc = QByteArray("global");
    const char* src = ssrc.constData();
    QByteArray smsg = message.toUtf8();
    const char* msg = smsg.constData();

    switch (level) {
        case QWebEnginePage::InfoMessageLevel:
            QMessageLogger(src, lineNumber, nullptr, "JavaScript").info() << msg;
            break;
        case QWebEnginePage::WarningMessageLevel:
            QMessageLogger(src, lineNumber, nullptr, "JavaScript").warning() << msg;
            break;
        case QWebEnginePage::ErrorMessageLevel:
            QMessageLogger(src, lineNumber, nullptr, "JavaScript").critical() << msg;
            break;
    }
}

CSpecLogHighlighter::CSpecLogHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{

}

void CSpecLogHighlighter::highlightBlock(const QString &text)
{
    formatBlock(text,QRegExp(QStringLiteral("^\\S{,8}"),
                             Qt::CaseInsensitive),Qt::black,true);
    formatBlock(text,QRegExp(QStringLiteral("\\s(\\S+\\s)?Debug:\\s"),
                             Qt::CaseInsensitive),Qt::black,true);
    formatBlock(text,QRegExp(QStringLiteral("\\s(\\S+\\s)?Warning:\\s"),
                             Qt::CaseInsensitive),Qt::darkRed,true);
    formatBlock(text,QRegExp(QStringLiteral("\\s(\\S+\\s)?Critical:\\s"),
                             Qt::CaseInsensitive),Qt::red,true);
    formatBlock(text,QRegExp(QStringLiteral("\\s(\\S+\\s)?Fatal:\\s"),
                             Qt::CaseInsensitive),Qt::red,true);
    formatBlock(text,QRegExp(QStringLiteral("\\s(\\S+\\s)?Info:\\s"),
                             Qt::CaseInsensitive),Qt::darkBlue,true);
    formatBlock(text,QRegExp(QStringLiteral("\\sBLOCKED\\s"),
                             Qt::CaseSensitive),Qt::darkRed,true);
    formatBlock(text,QRegExp(QStringLiteral("\\(\\S+\\)$"),
                             Qt::CaseInsensitive),Qt::gray,false,true);
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

CSpecUrlInterceptor::CSpecUrlInterceptor(QObject *p)
    : QWebEngineUrlRequestInterceptor(p)
{

}

void CSpecUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    QString rule;
    const QUrl url = info.requestUrl();

    // AdBlock
    if (gSet->isUrlBlocked(url,rule)) {
        if (gSet->settings.debugNetReqLogging)
            qWarning() << "Net request:" << url << "BLOCKED" <<
                          tr("(rule: '%1')").arg(rule);

        info.block(true);

        return;
    }

    // NoScript
    auto resource = info.resourceType();
    if (resource == QWebEngineUrlRequestInfo::ResourceTypeScript ||
            resource == QWebEngineUrlRequestInfo::ResourceTypeWorker ||
            resource == QWebEngineUrlRequestInfo::ResourceTypeSharedWorker ||
            resource == QWebEngineUrlRequestInfo::ResourceTypeServiceWorker ||
            resource == QWebEngineUrlRequestInfo::ResourceTypeUnknown) {

        if (gSet->isScriptBlocked(url,info.firstPartyUrl())) {

            if (gSet->settings.debugNetReqLogging)
                qWarning() << "Net request:" << url << "BLOCKED by NOSCRIPT";

            info.block(true);

            return;

        }
    }

    if (gSet->settings.debugNetReqLogging)
        qInfo() << "Net request:" << url;
}

CSpecUrlHistoryModel::CSpecUrlHistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{

}

int CSpecUrlHistoryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    if (gSet->mainHistory.count()>100)
        return 100;

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

CGDTextBrowser::CGDTextBrowser(QWidget *parent)
    : QTextBrowser(parent)
{

}

QVariant CGDTextBrowser::loadResource(int type, const QUrl &url)
{
    if (gSet && url.scheme().toLower()==QStringLiteral("gdlookup")) {
        QByteArray rplb;

        CIOEventLoop ev;
        QString mime;

        QUrlQuery qr(url);
        if ( qr.queryItemValue( QStringLiteral("blank") ) == QStringLiteral("1") ) {
            rplb = makeSimpleHtml(QString(),QString()).toUtf8();
            return rplb;
        }

        sptr<Dictionary::DataRequest> dr = gSet->dictNetMan->getResource(url,mime);

        connect(dr.get(), &Dictionary::DataRequest::finished,&ev,&CIOEventLoop::finished);
        QTimer::singleShot(15000,&ev,&CIOEventLoop::timeout);

        int ret = ev.exec();

        if (ret==1) { // Timeout
            rplb = makeSimpleHtml(tr("Error"),
                                  tr("Dictionary request timeout for query '%1'.")
                                  .arg(url.toString())).toUtf8();

        } else if (dr->isFinished() && dr->dataSize()>0 && ret==0) { // Dictionary success
            std::vector<char> vc = dr->getFullData();
            rplb = QByteArray(reinterpret_cast<const char*>(vc.data()), vc.size());

        } else { // Dictionary error
            rplb = makeSimpleHtml(tr("Error"),
                                  tr("Dictionary request failed for query '%1'.<br/>Error: %2.")
                                  .arg(url.toString(),dr->getErrorString())).toUtf8();
        }
        return rplb;
    }

    return QTextBrowser::loadResource(type,url);
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

CNetworkCookieJar::CNetworkCookieJar(QObject *parent)
    : QNetworkCookieJar(parent)
{

}

QList<QNetworkCookie> CNetworkCookieJar::getAllCookies()
{
    return allCookies();
}

void CNetworkCookieJar::initAllCookies(const QList<QNetworkCookie> & cookies)
{
    setAllCookies(cookies);
}

CFaviconLoader::CFaviconLoader(QObject *parent, const QUrl& url)
    : QObject(parent)
{
    m_url = url;
}

void CFaviconLoader::queryStart(bool forceCached)
{
    if (gSet->favicons.contains(m_url.host()+m_url.path())) {
        emit gotIcon(gSet->favicons.value(m_url.host()+m_url.path()));
        deleteLater();
        return;
    }

    if (gSet->favicons.contains(m_url.host())) {
        emit gotIcon(gSet->favicons.value(m_url.host()));
        deleteLater();
        return;
    }

    if (forceCached) {
        deleteLater();
        return;
    }

    if (m_url.isLocalFile()) {
        QIcon icon = QIcon(m_url.toLocalFile());
        if (!icon.isNull()) {
            emit gotIcon(icon);
            deleteLater();
        }

    } else {
        QNetworkReply* rpl = gSet->auxNetManager->get(QNetworkRequest(m_url));
        connect(rpl,&QNetworkReply::finished,this,&CFaviconLoader::queryFinished);
    }
}

void CFaviconLoader::queryFinished()
{
    auto rpl = qobject_cast<QNetworkReply*>(sender());
    if (rpl==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QPixmap p;
        if (p.loadFromData(rpl->readAll())) {
            QIcon ico(p);
            emit gotIcon(ico);
            QString host = rpl->url().host();
            QString path = rpl->url().path();
            if (!host.isEmpty()) {
                gSet->favicons[host] = ico;
                if (!path.isEmpty())
                    gSet->favicons[host+path] = ico;
            }
        }
    }

    rpl->deleteLater();
    deleteLater();
}
