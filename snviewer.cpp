#include <QShortcut>
#include <QUrl>
#include <QFileInfo>

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QUrlQuery>
#endif

#include <math.h>
#include "snviewer.h"
#include "mainwindow.h"
#include "specwidgets.h"
#include "globalcontrol.h"

CSnippetViewer::CSnippetViewer(CMainWindow* parent, QUrl aUri, QStringList aSearchText, bool setFocused,
                               QString AuxContent, QString zoom, bool startPage)
    : QWidget(parent)
{
	setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);
    parentWnd = parent;

    txtPanel->layout()->removeWidget(txtBrowser);
    txtBrowser->setParent(NULL);
    delete txtBrowser;
    txtBrowser = new QSpecWebView(txtPanel,parent);
    txtBrowser->setObjectName(QString::fromUtf8("txtBrowser"));
    txtBrowser->setUrl(QUrl("about:blank"));
    txtPanel->layout()->addWidget(txtBrowser);
    txtBrowser->setPage(new QSpecWebPage());

	tabWidget=NULL;
    tabTitle="";
    isStartPage = startPage;
    translationBkgdFinished=false;
    loadingBkgdFinished=false;
	loadingByWebKit = false;
    fileChanged = false;
    calculatedUrl="";
	onceLoaded=false;
    onceTranslated=false;
	Uri = aUri;
    firstUri = aUri;
    auxContent = AuxContent;
	lastFrame=NULL;
    slist.clear();
    savedBaseUrl.clear();
    backHistory.clear();
    forwardStack.clear();
    barLoading->setValue(0);
    barLoading->hide();
    searchEdit->show();
    if (!aSearchText.isEmpty()) slist.append(aSearchText);
	if (slist.count()>0) {
        searchEdit->addItems(aSearchText);
        searchEdit->setCurrentIndex(0);
	}
    bindToTab(parent->tabMain, setFocused);

    navButton->hide();

	txtBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
    txtBrowser->page()->setNetworkAccessManager(&(gSet->netAccess));
    txtBrowser->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);

    netHandler = new CSnNet(this);
    ctxHandler = new CSnCtxHandler(this);
    transHandler = new CSnTrans(this);
    msgHandler = new CSnMsgHandler(this);

    connect(backButton, SIGNAL(clicked()), msgHandler, SLOT(searchBack()));
    connect(fwdButton, SIGNAL(clicked()), msgHandler, SLOT(searchFwd()));
    connect(comboZoom,SIGNAL(currentIndexChanged(QString)), msgHandler, SLOT(setZoom(QString)));
    connect(transButton, SIGNAL(clicked()), transHandler, SLOT(translate()));
    connect(homeButton, SIGNAL(clicked()), msgHandler, SLOT(loadHomeUri()));
    connect(stopButton, SIGNAL(clicked()), txtBrowser, SLOT(stop()));
    connect(stopButton, SIGNAL(clicked()), netHandler, SLOT(netStop()));
    connect(reloadButton, SIGNAL(clicked()), netHandler, SLOT(reloadMedia()));
    connect(searchEdit->lineEdit(), SIGNAL(returnPressed()), fwdButton, SLOT(click()));
    connect(urlEdit->lineEdit(), SIGNAL(returnPressed()), this, SLOT(navByUrl()));
    connect(urlEdit->lineEdit(), SIGNAL(textEdited(QString)),msgHandler,SLOT(urlEdited(QString)));
    connect(navButton, SIGNAL(clicked()), msgHandler, SLOT(navByClick()));
    connect(fwdNavButton, SIGNAL(clicked()), msgHandler, SLOT(navForward()));
    connect(backNavButton, SIGNAL(clicked()), msgHandler, SLOT(navBack()));

    connect(txtBrowser, SIGNAL(loadProgress(int)), transHandler, SLOT(progressLoad(int)));
    connect(txtBrowser, SIGNAL(customContextMenuRequested(QPoint)), ctxHandler,SLOT(contextMenu(QPoint)));
    connect(txtBrowser, SIGNAL(loadFinished(bool)), netHandler, SLOT(loadFinished(bool)));
    connect(txtBrowser, SIGNAL(loadStarted()), netHandler, SLOT(loadStarted()));
    connect(txtBrowser, SIGNAL(titleChanged(QString)), this, SLOT(titleChanged(QString)));
    connect(txtBrowser, SIGNAL(urlChanged(QUrl)), this, SLOT(urlChanged(QUrl)));

    connect(txtBrowser->page(), SIGNAL(linkClickedExt(QWebFrame*,QUrl)), msgHandler,
            SLOT(linkClicked(QWebFrame*,QUrl)));
    connect(txtBrowser->page(), SIGNAL(linkHovered(QString,QString,QString)), msgHandler,
            SLOT(linkHovered(QString,QString,QString)));
    connect(txtBrowser->page(), SIGNAL(statusBarMessage(QString)),this,SLOT(statusBarMsg(QString)));

    backNavButton->setIcon(QIcon::fromTheme("go-previous"));
    fwdNavButton->setIcon(QIcon::fromTheme("go-next"));
    backButton->setIcon(QIcon::fromTheme("go-up-search"));
	fwdButton->setIcon(QIcon::fromTheme("go-down-search"));
	homeButton->setIcon(QIcon::fromTheme("go-home"));
	reloadButton->setIcon(QIcon::fromTheme("view-refresh"));
    stopButton->setIcon(QIcon::fromTheme("process-stop"));
    navButton->setIcon(QIcon::fromTheme("arrow-right"));

    backNavButton->setEnabled(false);
    fwdNavButton->setEnabled(false);
    stopButton->setEnabled(false);
	reloadButton->setEnabled(false);

    QShortcut* sc;
    sc = new QShortcut(QKeySequence(Qt::Key_Slash),this);
    connect(sc,SIGNAL(activated()),searchEdit,SLOT(setFocus()));

    waitDlg = new CWaitDlg();
    waitDlg->hide();
    QVBoxLayout* wb = qobject_cast<QVBoxLayout *>(layout());
    if (wb!=NULL)
        wb->insertWidget(wb->indexOf(txtPanel),waitDlg);

    netHandler->reloadMedia(true);
    txtBrowser->setFocus();

    int idx = comboZoom->findText(zoom,Qt::MatchExactly);
    if (idx>=0) {
        comboZoom->setCurrentIndex(idx);
    } else {
        comboZoom->addItem(zoom);
        comboZoom->setCurrentIndex(comboZoom->findText(zoom,Qt::MatchExactly));
    }

    int mv = round((70*parentWnd->height()/100)/(urlEdit->fontMetrics().height()));
    urlEdit->setMaxVisibleItems(mv);

    if (!startPage)
        parentWnd->closeStartPage();

    parentWnd->updateSuggestionLists(this);
}

CSnippetViewer::~CSnippetViewer()
{
    waitDlg->deleteLater();
}

void CSnippetViewer::updateButtonsState()
{
    stopButton->setEnabled(loadingByWebKit);
    reloadButton->setEnabled(!loadingByWebKit && !Uri.isEmpty());
    fwdNavButton->setEnabled(forwardStack.count()>0);
    backNavButton->setEnabled(backHistory.count()>1);
}

void CSnippetViewer::navByUrl()
{
    navByUrl(urlEdit->lineEdit()->text());
}

void CSnippetViewer::navByUrl(QString url)
{
    navButton->hide();

    QString aUrl = url;
    QUrl u = QUrl::fromUserInput(aUrl);

    if (!u.isValid() || !url.contains('.')) {
        u = QUrl("http://google.com/search");
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
        u.addQueryItem("q", url);
#else
        QUrlQuery qu;
        qu.addQueryItem("q", url);
        u.setQuery(qu);
#endif
    }

    urlEdit->setPalette(QApplication::palette(urlEdit));

    fileChanged=false;
    Uri=u;
    forwardStack.clear();
    netHandler->loadProcessed(u);
}

void CSnippetViewer::titleChanged(const QString & title)
{
    int i = tabWidget->indexOf(this);
	QString s = title;
    QUrl uri;
	if (s.isEmpty()) {
        if (txtBrowser->page()->mainFrame()!=NULL) {
            uri = txtBrowser->page()->mainFrame()->baseUrl();
        }
        if (uri.isEmpty()) uri = Uri;
        s=uri.toLocalFile();
		if (!s.isEmpty()) {
			s=QFileInfo(s).fileName();
		} else {
            QStringList sl = uri.path().split('/');
            if (!sl.isEmpty())
                s=sl.last();
            else
                s=uri.toString();
		}
        if (s.isEmpty()) s = "< ... >";
	}
    if (!title.isEmpty())
        tabTitle = title;
    else
        tabTitle = s;
    tabWidget->setTabText(i,tabWidget->fontMetrics().elidedText(s,Qt::ElideRight,150));
//	tabWidget->setTabToolTip(i,s);

    if (!title.isEmpty()) {
		if (txtBrowser->page()!=NULL)
			if (txtBrowser->page()->mainFrame()!=NULL &&
					txtBrowser->page()->mainFrame()->baseUrl().isValid() &&
					!txtBrowser->page()->mainFrame()->baseUrl().toString().contains("about:blank",Qt::CaseInsensitive)) {
                UrlHolder uh(title,txtBrowser->page()->mainFrame()->baseUrl());
                gSet->appendMainHistory(uh);
            }
    }
    parentWnd->updateTitle();
	parentWnd->updateTabs();
}

void CSnippetViewer::urlChanged(const QUrl & url)
{
    if (url.toString().contains("about:blank",Qt::CaseInsensitive))
        urlEdit->setEditText(Uri.toString());
    else
        urlEdit->setEditText(url.toString());
    if (fileChanged) {
        urlEdit->setToolTip(tr("File changed. Temporary copy loaded in memory."));
        QPalette p = urlEdit->palette();
        p.setBrush(QPalette::Base,QBrush(QColor("#D7FFD7")));
        urlEdit->setPalette(p);
    } else {
        urlEdit->setToolTip(tr("Displayed URL"));
        urlEdit->setPalette(QApplication::palette(urlEdit));
    }
}

void CSnippetViewer::recycleTab()
{
    if (tabTitle.isEmpty() || !txtBrowser->page()->mainFrame()->baseUrl().isValid() ||
            txtBrowser->page()->mainFrame()->baseUrl().toString().
            contains("about:blank",Qt::CaseInsensitive)) return;
    gSet->appendRecycled(tabTitle,txtBrowser->page()->mainFrame()->baseUrl());
}

void CSnippetViewer::statusBarMsg(const QString &msg)
{
    parentWnd->statusBar()->showMessage(msg);
}

QString CSnippetViewer::getDocTitle()
{
    QString s = txtBrowser->title();
    if (s.isEmpty()) {
        QStringList f = Uri.path().split('/');
        if (!f.isEmpty())
            s = f.last();
        else
            s = Uri.toString();
    }
    return s;
}

void CSnippetViewer::keyPressEvent(QKeyEvent *event)
{
    if (event->key()==Qt::Key_F5)
        closeTab();
    else if (event->key()==Qt::Key_F3)
        msgHandler->searchFwd();
    else if (event->key()==Qt::Key_F2)
        msgHandler->searchBack();
    else
        QWidget::keyPressEvent(event);
}

void CSnippetViewer::bindToTab(QSpecTabWidget* tabs, bool setFocused)
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

void CSnippetViewer::closeTab(bool nowait)
{
    if (waitDlg->isVisible()) return; // prevent closing while translation thread active
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

void CSnippetViewer::updateWebViewAttributes()
{
    txtBrowser->settings()->setAttribute(QWebSettings::AutoLoadImages,
                                              QWebSettings::globalSettings()->testAttribute(QWebSettings::AutoLoadImages));

    if (gSet->overrideStdFonts) {
        txtBrowser->settings()->setFontFamily(QWebSettings::StandardFont,gSet->fontStandard);
        txtBrowser->settings()->setFontFamily(QWebSettings::FixedFont,gSet->fontFixed);
        txtBrowser->settings()->setFontFamily(QWebSettings::SerifFont,gSet->fontSerif);
        txtBrowser->settings()->setFontFamily(QWebSettings::SansSerifFont,gSet->fontSansSerif);
    }
}

void CSnippetViewer::updateHistorySuggestion(const QStringList& suggestionList)
{
    QString s = urlEdit->currentText();
    urlEdit->clear();
    urlEdit->addItems(suggestionList);
    urlEdit->setEditText(s);
}
