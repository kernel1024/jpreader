#include <QTextCharFormat>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QMessageLogger>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
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
    m_tabBar = new CSpecTabBar(this);
    setTabBar(m_tabBar);
    connect(m_tabBar, &CSpecTabBar::tabRightClicked,this,&CSpecTabWidget::tabRightClick);
    connect(m_tabBar, &CSpecTabBar::tabLeftClicked,this,&CSpecTabWidget::tabLeftClick);
}

void CSpecTabWidget::tabLeftClick(int index)
{
    Q_EMIT tabLeftClicked(index);
}

void CSpecTabWidget::tabRightClick(int index)
{
    Q_EMIT tabRightClicked(index);
    if (mainTabWidget) {
        auto tb = qobject_cast<CSpecTabContainer *>(widget(index));
        if (tb && tb->parentWnd()) {
            if (!tb->parentWnd()->isTitleRenameTimerActive())
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
        Q_EMIT tooltipRequested(gpos,lpos);
        event->accept();
        return true;
    }

    return QTabWidget::event(event);
}

void CSpecTabWidget::createTab()
{
    if (mainTabWidget && parentWnd())
        parentWnd()->openEmptyBrowser();
}

void CSpecTabWidget::selectNextTab()
{
    if (count()==0) return;
    int idx = currentIndex();
    idx++;
    if (idx>=count())
        idx = 0;
    setCurrentIndex(idx);
}

void CSpecTabWidget::selectPrevTab()
{
    if (count()==0) return;
    int idx = currentIndex();
    idx--;
    if (idx<0)
        idx = count()-1;
    setCurrentIndex(idx);
}

CSpecTabBar::CSpecTabBar(CSpecTabWidget *p)
    : QTabBar(p)
{
    m_tabWidget = p;
    m_dragStart = QPoint(0,0);
    m_draggingTab = nullptr;
}

CSpecTabBar::CSpecTabBar(QWidget *p)
    : QTabBar(p)
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
                Q_EMIT tabRightClicked(i);
                return;
            }
        } else if (event->button()==Qt::LeftButton) {
            if (tabRect(i).contains(event->pos())) {
                if (m_tabWidget) {
                    m_draggingTab = qobject_cast<CSpecTabContainer *>(m_tabWidget->widget(i));
                    m_dragStart = event->pos();
                }
                Q_EMIT tabLeftClicked(i);
            }
        }
    }
    QTabBar::mousePressEvent(event);
    tabCount = count();
    for (int i=0;i<tabCount;i++) {
        if (btn==Qt::RightButton) {
            if (tabRect(i).contains(pos)) {
                Q_EMIT tabRightPostClicked(i);
            }
        } else if (btn==Qt::LeftButton) {
            if (tabRect(i).contains(pos)) {
                Q_EMIT tabLeftPostClicked(i);
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
    const QMargins mainWindowDecorationsSafeMargins(50,50,50,100);

    if ((m_draggingTab!=nullptr) && (m_tabWidget!=nullptr) &&
            (event->buttons() == Qt::LeftButton)) {

        QRect wr = QRect(mapToGlobal(QPoint(0,0)),size()).marginsAdded(mainWindowDecorationsSafeMargins);

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

CSpecMenuStyle::CSpecMenuStyle(QStyle *style)
    : QProxyStyle(style)
{

}

int CSpecMenuStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                              QStyleHintReturn *returnData) const
{
    if ( hint == SH_DrawMenuBarSeparator)
        return static_cast<int>(true);

    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

CSpecTabContainer::CSpecTabContainer(QWidget *parent)
    : QWidget(parent)
{
    m_parentWnd = qobject_cast<CMainWindow *>(parent);
}

void CSpecTabContainer::bindToTab(CSpecTabWidget *tabs, bool setFocused)
{
    m_tabWidget=tabs;
    if (m_tabWidget==nullptr) return;
    int i = m_tabWidget->addTab(this,m_tabTitle);
    if (gSet->settings()->showTabCloseButtons) {
        auto b = new QPushButton(QIcon::fromTheme(QSL("dialog-close")),QString());
        b->setFlat(true);
        int sz = m_tabWidget->tabBar()->fontMetrics().height();
        b->resize(QSize(sz,sz));
        connect(b, &QPushButton::clicked,this,&CSpecTabContainer::closeTab);
        m_tabWidget->tabBar()->setTabButton(i,QTabBar::RightSide,b);
    }
    if (setFocused)
        setTabFocused();
    m_parentWnd->updateTitle();
    m_parentWnd->updateTabs();
}

void CSpecTabContainer::setTabTitle(const QString &title)
{
    const int tabTitleElidingWidth = 150;

    m_tabTitle = title;
    int idx = m_parentWnd->tabMain->indexOf(this);
    if (idx>=0 && idx<m_parentWnd->tabMain->count()) {
        m_parentWnd->startTitleRenameTimer();
        m_parentWnd->tabMain->setTabText(idx,m_tabWidget->fontMetrics()
                                         .elidedText(m_tabTitle,Qt::ElideRight,tabTitleElidingWidth));
    }
}

void CSpecTabContainer::updateTabIcon(const QIcon &icon)
{
    if (!gSet->webProfile()->settings()->
            testAttribute(QWebEngineSettings::AutoLoadIconsForPage)) return;
    if (m_tabWidget==nullptr) return;
    m_tabWidget->setTabIcon(m_tabWidget->indexOf(this),icon);
}

void CSpecTabContainer::setTabFocused()
{
    if (m_tabWidget==nullptr) return;
    m_tabWidget->setCurrentWidget(this);
}

void CSpecTabContainer::detachTab()
{
    if (m_tabWidget->count()<=1) return;

    // Reparenting
    CMainWindow* mwnd = gSet->addMainWindowEx(false,false);
    m_tabWidget->removeTab(m_tabWidget->indexOf(this));
    m_parentWnd = mwnd;
    setParent(mwnd);
    bindToTab(mwnd->tabMain);
    activateWindow();
    mwnd->raise();
}

void CSpecTabContainer::closeTab(bool nowait)
{
    if (!canClose()) return;
    if (m_tabWidget->count()<=1) return; // prevent closing while only 1 tab remains

    if (!nowait) {
        if (gSet->isBlockTabCloseActive()) return;
        gSet->blockTabClose();
    }
    if (m_tabWidget) {
        if ((m_parentWnd->lastTabIndex()>=0) &&
                (m_parentWnd->lastTabIndex()<m_tabWidget->count())) {
            m_tabWidget->setCurrentIndex(m_parentWnd->lastTabIndex());
        }
        m_tabWidget->removeTab(m_tabWidget->indexOf(this));
    }
    recycleTab();
    m_parentWnd->updateTitle();
    m_parentWnd->updateTabs();
    m_parentWnd->checkTabs();
    deleteLater();
}

CSpecWebView::CSpecWebView(QWidget *parent)
    : QWebEngineView(parent)
{
    QWidget * w = parent;
    while (w) {
        auto viewer = qobject_cast<CSnippetViewer *>(w);
        if (viewer) {
            parentViewer = viewer;
            break;
        }
        w = w->parentWidget();
    }
    if (parentViewer==nullptr)
        qCritical() << "parentViewer is nullptr";

    m_page = new CSpecWebPage(gSet->webProfile(), this);
    setPage(m_page);
}

CSpecWebPage *CSpecWebView::customPage() const
{
    return m_page;
}

void CSpecWebView::setHtmlInterlocked(const QString &html, const QUrl &baseUrl)
{
    static QMutex dataUrlLock;
    QMutexLocker locker(&dataUrlLock);

    setHtml(html,baseUrl);
}

QWebEngineView *CSpecWebView::createWindow(QWebEnginePage::WebWindowType type)
{
    if (parentViewer==nullptr) return nullptr;

    auto sv = new CSnippetViewer(parentViewer->parentWnd(),QUrl(),QStringList(),
                                            (type!=QWebEnginePage::WebBrowserBackgroundTab));
    return sv->txtBrowser;
}

void CSpecWebView::contextMenuEvent(QContextMenuEvent *event)
{
    if (parentViewer)
        Q_EMIT contextMenuRequested(event->pos(), m_page->contextMenuData());
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
    Q_EMIT linkClickedExt(url,static_cast<int>(type),isMainFrame);

    if (gSet->settings()->debugNetReqLogging) {
        if (isMainFrame) {
            qInfo() << "Net request (main frame):" << url;
        } else {
            qInfo() << "Net request (sub frame):" << url;
        }
    }

    bool blocked = gSet->isUrlBlocked(url);

    if (!blocked) {
        // NoScript discovery reload
        gSet->clearNoScriptPageHosts(url.toString(CSettings::adblockUrlFmt));

        // Userscripts
        const QVector<CUserScript> sl(gSet->getUserScriptsForUrl(url, isMainFrame, false, false));

        if (!sl.isEmpty())
            scripts().clear();

        for (int i = 0; i < sl.count(); ++i)
        {
            QWebEngineScript::InjectionPoint injectionPoint(QWebEngineScript::DocumentReady);

            if (sl.at(i).getInjectionTime() == CUserScript::DocumentCreationTime) {
                injectionPoint = QWebEngineScript::DocumentCreation;

            } else if (sl.at(i).getInjectionTime() == CUserScript::DeferredTime) {
                injectionPoint = QWebEngineScript::Deferred;
            }

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

    return gSet->settings()->ignoreSSLErrors;
}

void CSpecWebPage::javaScriptConsoleMessage(QWebEnginePage::JavaScriptConsoleMessageLevel level,
                                            const QString &message, int lineNumber,
                                            const QString &sourceID)
{
    if (!gSet->settings()->jsLogConsole) return;

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
    formatBlock(text,QRegularExpression(QSL("^\\S{,8}"),
                             QRegularExpression::CaseInsensitiveOption),Qt::black,true);
    formatBlock(text,QRegularExpression(QSL("\\s(\\S+\\s)?Debug:\\s"),
                             QRegularExpression::CaseInsensitiveOption),Qt::black,true);
    formatBlock(text,QRegularExpression(QSL("\\s(\\S+\\s)?Warning:\\s"),
                             QRegularExpression::CaseInsensitiveOption),Qt::darkRed,true);
    formatBlock(text,QRegularExpression(QSL("\\s(\\S+\\s)?Critical:\\s"),
                             QRegularExpression::CaseInsensitiveOption),Qt::red,true);
    formatBlock(text,QRegularExpression(QSL("\\s(\\S+\\s)?Fatal:\\s"),
                             QRegularExpression::CaseInsensitiveOption),Qt::red,true);
    formatBlock(text,QRegularExpression(QSL("\\s(\\S+\\s)?Info:\\s"),
                             QRegularExpression::CaseInsensitiveOption),Qt::darkBlue,true);
    formatBlock(text,QRegularExpression(QSL("\\sBLOCKED\\s")),Qt::darkRed,true);
    formatBlock(text,QRegularExpression(QSL("\\(\\S+\\)$"),
                             QRegularExpression::CaseInsensitiveOption),Qt::gray,false,true);
}

void CSpecLogHighlighter::formatBlock(const QString &text,
                                      const QRegularExpression &exp,
                                      const QColor &color,
                                      bool weight,
                                      bool italic,
                                      bool underline,
                                      bool strikeout)
{
    if (text.isEmpty()) return;

    QTextCharFormat fmt;
    fmt.setForeground(color);
    if (weight) {
        fmt.setFontWeight(QFont::Bold);
    } else {
        fmt.setFontWeight(QFont::Normal);
    }
    fmt.setFontItalic(italic);
    fmt.setFontUnderline(underline);
    fmt.setFontStrikeOut(strikeout);

    auto it = exp.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), fmt);
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
        if (gSet->settings()->debugNetReqLogging) {
            qWarning() << "Net request:" << url << "BLOCKED" <<
                          tr("(rule: '%1')").arg(rule);
        }

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

            if (gSet->settings()->debugNetReqLogging)
                qWarning() << "Net request:" << url << "BLOCKED by NOSCRIPT";

            info.block(true);

            return;

        }
    }

    if (gSet->settings()->debugNetReqLogging)
        qInfo() << "Net request:" << url;
}

CSpecUrlHistoryModel::CSpecUrlHistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{

}

int CSpecUrlHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    const int maxHistoryLength = 100;

    if (gSet->mainHistory().count()>maxHistoryLength)
        return maxHistoryLength;

    return gSet->mainHistory().count();
}

QVariant CSpecUrlHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    if (role==Qt::DisplayRole || role==Qt::EditRole)
        return gSet->mainHistory().at(index.row()).url.toString();

    return QVariant();
}

Qt::ItemFlags CSpecUrlHistoryModel::flags(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return Qt::NoItemFlags;

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

CGDTextBrowser::CGDTextBrowser(QWidget *parent)
    : QTextBrowser(parent)
{

}

QVariant CGDTextBrowser::loadResource(int type, const QUrl &url)
{
    const int gdDictionaryResponseTimeout = 10000;

    if (gSet!=nullptr && url.scheme().toLower()==QSL("gdlookup")) {
        QByteArray rplb;

        CIOEventLoop ev;
        QString mime;

        QUrlQuery qr(url);
        if ( qr.queryItemValue( QSL("blank") ) == QSL("1") ) {
            rplb = CGenericFuncs::makeSimpleHtml(QString(),QString()).toUtf8();
            return rplb;
        }

        sptr<Dictionary::DataRequest> dr = gSet->dictionaryNetworkAccessManager()->getResource(url,mime);

        connect(dr.get(), &Dictionary::DataRequest::finished,&ev,&CIOEventLoop::finished);
        QTimer::singleShot(gdDictionaryResponseTimeout,&ev,&CIOEventLoop::timeout);

        int ret = ev.exec();

        if (ret==1) { // Timeout
            rplb = CGenericFuncs::makeSimpleHtml(tr("Error"),
                                                 tr("Dictionary request timeout for query '%1'.")
                                                 .arg(url.toString())).toUtf8();

        } else if (dr->isFinished() && dr->dataSize()>0 && ret==0) { // Dictionary success
            std::vector<char> vc = dr->getFullData();
            rplb = QByteArray(reinterpret_cast<const char*>(vc.data()),
                              static_cast<int>(vc.size()));

        } else { // Dictionary error
            rplb = CGenericFuncs::makeSimpleHtml(tr("Error"),
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
    if (gSet->favicons().contains(m_url.host()+m_url.path())) {
        Q_EMIT gotIcon(gSet->favicons().value(m_url.host()+m_url.path()));
        Q_EMIT finished();
        return;
    }

    if (gSet->favicons().contains(m_url.host())) {
        Q_EMIT gotIcon(gSet->favicons().value(m_url.host()));
        Q_EMIT finished();
        return;
    }

    if (forceCached) {
        Q_EMIT finished();
        return;
    }

    if (m_url.isLocalFile()) {
        QIcon icon = QIcon(m_url.toLocalFile());
        if (!icon.isNull()) {
            Q_EMIT gotIcon(icon);
            Q_EMIT finished();
        }

    } else {
        QNetworkRequest req(m_url);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);
        connect(rpl,&QNetworkReply::finished,this,&CFaviconLoader::queryFinished);
    }
}

void CFaviconLoader::queryFinished()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply*>(sender()));
    if (rpl.isNull()) {
        Q_EMIT finished();
        return;
    }

    if (rpl->error() == QNetworkReply::NoError) {
        QPixmap p;
        if (p.loadFromData(rpl->readAll())) {
            QIcon ico(p);
            Q_EMIT gotIcon(ico);
            QString host = rpl->url().host();
            QString path = rpl->url().path();
            if (!host.isEmpty()) {
                gSet->addFavicon(host,ico);
                if (!path.isEmpty())
                    gSet->addFavicon(host+path,ico);
            }
        }
    }

    Q_EMIT finished();
}
