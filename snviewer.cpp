#include <QShortcut>
#include <QUrl>
#include <QFileInfo>
#include <QTimer>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QCompleter>
#include <QUrlQuery>

#include <math.h>
#include "snviewer.h"
#include "mainwindow.h"
#include "specwidgets.h"
#include "globalcontrol.h"

CSnippetViewer::CSnippetViewer(CMainWindow* parent, QUrl aUri, QStringList aSearchText, bool setFocused,
                               QString AuxContent, QString zoom, bool startPage)
    : CSpecTabContainer(parent)
{
	setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);

    layout()->removeWidget(browser);
    browser->setParent(NULL);
    delete browser;

    txtBrowser = new QWebEngineView(this);
    txtBrowser->setObjectName(QString::fromUtf8("txtBrowser"));
    txtBrowser->setUrl(QUrl("about://blank"));
    layout()->addWidget(txtBrowser);
    CSpecWebPage *wp = new CSpecWebPage(gSet->webProfile,this);
    txtBrowser->setPage(wp);
    txtBrowser->page()->action(QWebEnginePage::Paste)->setIcon(QIcon::fromTheme("edit-paste"));
    txtBrowser->page()->action(QWebEnginePage::SelectAll)->setIcon(QIcon::fromTheme("edit-select-all"));

    tabTitle="";
    isStartPage = startPage;
    translationBkgdFinished=false;
    loadingBkgdFinished=false;
    loading = false;
    fileChanged = false;
    calculatedUrl="";
    onceTranslated=false;
    requestAutotranslate=false;
    searchList.clear();
    barLoading->setValue(0);
    barLoading->hide();
    barPlaceholder->show();
    if (!aSearchText.isEmpty()) searchList.append(aSearchText);
    if (searchList.count()>0) {
        searchEdit->addItems(aSearchText);
        searchEdit->setCurrentIndex(0);
	}
    bindToTab(parent->tabMain, setFocused);

    txtBrowser->setContextMenuPolicy(Qt::CustomContextMenu);

    netHandler = new CSnNet(this);
    ctxHandler = new CSnCtxHandler(this);
    transHandler = new CSnTrans(this);
    msgHandler = new CSnMsgHandler(this);
    waitHandler = new CSnWaitCtl(this);

    QCompleter *completer = new QCompleter(this);
    completer->setModel(new CSpecUrlHistoryModel(completer));
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    urlEdit->setCompleter(completer);

    connect(backButton, SIGNAL(clicked()), msgHandler, SLOT(searchBack()));
    connect(fwdButton, SIGNAL(clicked()), msgHandler, SLOT(searchFwd()));
    connect(comboZoom,SIGNAL(currentIndexChanged(QString)), msgHandler, SLOT(setZoom(QString)));
    connect(transButton, SIGNAL(clicked()), transHandler, SLOT(translate()));
    connect(stopButton, SIGNAL(clicked()), txtBrowser, SLOT(stop()));
    connect(reloadButton, SIGNAL(clicked()), txtBrowser, SLOT(reload()));
    connect(searchEdit->lineEdit(), SIGNAL(returnPressed()), fwdButton, SLOT(click()));
    connect(urlEdit, SIGNAL(returnPressed()), this, SLOT(navByUrl()));
    connect(navButton, SIGNAL(clicked()), msgHandler, SLOT(navByClick()));
    connect(fwdNavButton, SIGNAL(clicked()), txtBrowser, SLOT(forward()));
    connect(backNavButton, SIGNAL(clicked()), txtBrowser, SLOT(back()));

    connect(txtBrowser, SIGNAL(loadProgress(int)), transHandler, SLOT(progressLoad(int)));
    connect(txtBrowser, SIGNAL(customContextMenuRequested(QPoint)), ctxHandler,SLOT(contextMenu(QPoint)));
    connect(txtBrowser, SIGNAL(loadFinished(bool)), netHandler, SLOT(loadFinished(bool)));
    connect(txtBrowser, SIGNAL(loadStarted()), netHandler, SLOT(loadStarted()));
    connect(txtBrowser, SIGNAL(titleChanged(QString)), this, SLOT(titleChanged(QString)));
    connect(txtBrowser, SIGNAL(urlChanged(QUrl)), this, SLOT(urlChanged(QUrl)));
    connect(txtBrowser, SIGNAL(iconUrlChanged(QUrl)), netHandler, SLOT(iconUrlChanged(QUrl)));

#ifdef WEBENGINE_56
    connect(txtBrowser, SIGNAL(renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus,int)),
            msgHandler, SLOT(renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus,int)));
#endif

    connect(txtBrowser->page(), SIGNAL(authenticationRequired(QUrl,QAuthenticator*)),
            netHandler, SLOT(authenticationRequired(QUrl,QAuthenticator*)));
    connect(txtBrowser->page(), SIGNAL(proxyAuthenticationRequired(QUrl,QAuthenticator*,QString)),
            netHandler, SLOT(proxyAuthenticationRequired(QUrl,QAuthenticator*,QString)));

    connect(msgHandler->loadingBarHideTimer, SIGNAL(timeout()), barLoading, SLOT(hide()));
    connect(msgHandler->loadingBarHideTimer, SIGNAL(timeout()), barPlaceholder, SLOT(show()));

    connect(txtBrowser->page(), SIGNAL(linkHovered(QString)), msgHandler,
            SLOT(linkHovered(QString)));
    connect(wp, SIGNAL(linkClickedExt(QUrl,int,bool)), netHandler,
            SLOT(userNavigationRequest(QUrl,int,bool)));

    backNavButton->setIcon(QIcon::fromTheme("go-previous"));
    fwdNavButton->setIcon(QIcon::fromTheme("go-next"));
    backButton->setIcon(QIcon::fromTheme("go-up-search"));
	fwdButton->setIcon(QIcon::fromTheme("go-down-search"));
	reloadButton->setIcon(QIcon::fromTheme("view-refresh"));
    stopButton->setIcon(QIcon::fromTheme("process-stop"));
    navButton->setIcon(QIcon::fromTheme("arrow-right"));

    for (int i=0;i<LSCOUNT;i++) {
        QString s = gSet->getSourceLanguageIDStr(i,TE_GOOGLE);
        if (s.startsWith("ja"))
            s=QString("jp");
        comboSrcLang->addItem(s,i);
        switch (i) {
            case LS_JAPANESE: comboSrcLang->setItemData(i,"Japanese",Qt::ToolTipRole); break;
            case LS_CHINESESIMP: comboSrcLang->setItemData(i,"Chinese simplified",Qt::ToolTipRole); break;
            case LS_CHINESETRAD: comboSrcLang->setItemData(i,"Chinese traditional",Qt::ToolTipRole); break;
            case LS_KOREAN: comboSrcLang->setItemData(i,"Korean",Qt::ToolTipRole); break;
        }
    }
    for (int i=0;i<TECOUNT;i++)
        comboTranEngine->addItem(gSet->getTranslationEngineString(i),i);
    comboSrcLang->setCurrentIndex(gSet->getSourceLanguage());
    comboTranEngine->setCurrentIndex(gSet->translatorEngine);
    connect(comboSrcLang, SIGNAL(currentIndexChanged(int)), msgHandler, SLOT(srcLang(int)));
    connect(comboTranEngine, SIGNAL(currentIndexChanged(int)), msgHandler, SLOT(tranEngine(int)));
    connect(gSet->sourceLanguage, SIGNAL(triggered(QAction*)), msgHandler, SLOT(updateSrcLang(QAction*)));
    connect(gSet, SIGNAL(settingsUpdated()), msgHandler, SLOT(updateTranEngine()));

    QShortcut* sc;
    sc = new QShortcut(QKeySequence(Qt::Key_Slash),this);
    connect(sc,SIGNAL(activated()),searchPanel,SLOT(show()));
    sc = new QShortcut(QKeySequence(Qt::Key_F + Qt::CTRL),this);
    connect(sc,SIGNAL(activated()),searchPanel,SLOT(show()));

    waitPanel->hide();
    errorPanel->hide();
    errorLabel->setText(QString());
    searchPanel->setVisible(!searchList.isEmpty());

    if (AuxContent.isEmpty())
        netHandler->load(aUri);
    else
        netHandler->load(AuxContent);

    txtBrowser->setFocus();

    int idx = comboZoom->findText(zoom,Qt::MatchExactly);
    if (idx>=0) {
        comboZoom->setCurrentIndex(idx);
    } else {
        comboZoom->addItem(zoom);
        comboZoom->setCurrentIndex(comboZoom->findText(zoom,Qt::MatchExactly));
    }

    int mv = round((70*parentWnd->height()/100)/(urlEdit->fontMetrics().height()));
    completer->setMaxVisibleItems(mv);

    if (!startPage)
        parentWnd->closeStartPage();
}

void CSnippetViewer::updateButtonsState()
{
    stopButton->setEnabled(loading);
    reloadButton->setEnabled(!loading);
    fwdNavButton->setEnabled(txtBrowser->history()->canGoForward());
    backNavButton->setEnabled(txtBrowser->history()->canGoBack());
}

void CSnippetViewer::navByUrl()
{
    navByUrl(urlEdit->text());
}

void CSnippetViewer::navByUrl(QString url)
{
    QString aUrl = url;
    QUrl u = QUrl::fromUserInput(aUrl);

    if (u.scheme().toLower()!="gdlookup" && (!u.isValid() || !url.contains('.'))) {
        u = QUrl("http://google.com/search");
        QUrlQuery qu;
        qu.addQueryItem("q", url);
        u.setQuery(qu);
    }

    urlEdit->setStyleSheet(QString());

    fileChanged=false;

    netHandler->load(u);
}

void CSnippetViewer::titleChanged(const QString & title)
{
    int i = tabWidget->indexOf(this);
	QString s = title;
    QUrl uri;
	if (s.isEmpty()) {
        uri = getUrl();
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
    s = s.trimmed();
    if (!title.isEmpty())
        tabTitle = title.trimmed();
    else
        tabTitle = s;
    tabWidget->setTabText(i,tabWidget->fontMetrics().elidedText(s,Qt::ElideRight,150));

    if (!title.isEmpty()) {
        if (txtBrowser->page()->url().isValid() &&
                !txtBrowser->page()->url().toString().contains("about:blank",Qt::CaseInsensitive)) {
                UrlHolder uh(title,txtBrowser->page()->url());
                if (!gSet->updateMainHistoryTitle(uh,title.trimmed()))
                    gSet->appendMainHistory(uh);
            }
    }
    parentWnd->updateTitle();
	parentWnd->updateTabs();
}

void CSnippetViewer::urlChanged(const QUrl & url)
{
    if (url.scheme().startsWith("http",Qt::CaseInsensitive) ||
            url.scheme().startsWith("file",Qt::CaseInsensitive))
        urlEdit->setText(url.toString());
    else {
        QUrl aUrl = getUrl();
        if (url.scheme().startsWith("http",Qt::CaseInsensitive) ||
                url.scheme().startsWith("file",Qt::CaseInsensitive))
            urlEdit->setText(aUrl.toString());
        else if (netHandler->loadedUrl.isValid() &&
                 (netHandler->loadedUrl.scheme().startsWith("http",Qt::CaseInsensitive) ||
                                 netHandler->loadedUrl.scheme().startsWith("file",Qt::CaseInsensitive)))
            urlEdit->setText(netHandler->loadedUrl.toString());
        else
            urlEdit->clear();
    }

    if (fileChanged) {
        urlEdit->setToolTip(tr("File changed. Temporary copy loaded in memory."));
        urlEdit->setStyleSheet("QLineEdit { background: #d7ffd7; }");
    } else {
        urlEdit->setToolTip(tr("Displayed URL"));
        urlEdit->setStyleSheet(QString());
    }

    if (gSet->favicons.keys().contains(url.host()+url.path()))
        updateTabIcon(gSet->favicons[url.host()+url.path()]);
    else if (gSet->favicons.keys().contains(url.host()))
        updateTabIcon(gSet->favicons[url.host()]);
}

void CSnippetViewer::recycleTab()
{
    if (tabTitle.isEmpty() || !txtBrowser->page()->url().isValid() ||
            txtBrowser->page()->url().toString().
            startsWith("about",Qt::CaseInsensitive)) return;
    gSet->appendRecycled(tabTitle,txtBrowser->page()->url());
}

QUrl CSnippetViewer::getUrl()
{
    return txtBrowser->url();
}

void CSnippetViewer::setToolbarVisibility(bool visible)
{
    toolBar->setVisible(visible);
}

void CSnippetViewer::statusBarMsg(const QString &msg)
{
    parentWnd->statusBar()->showMessage(msg);
}

QString CSnippetViewer::getDocTitle()
{
    QString s = txtBrowser->title();
    if (s.isEmpty()) {
        QStringList f = getUrl().path().split('/');
        if (!f.isEmpty())
            s = f.last();
        else
            s = getUrl().toString();
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

void CSnippetViewer::updateWebViewAttributes()
{
    txtBrowser->settings()->setAttribute(
                QWebEngineSettings::AutoLoadImages,
                QWebEngineSettings::globalSettings()->testAttribute(
                    QWebEngineSettings::AutoLoadImages));

    if (gSet->overrideStdFonts) {
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::StandardFont,gSet->fontStandard);
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::FixedFont,gSet->fontFixed);
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::SerifFont,gSet->fontSerif);
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::SansSerifFont,gSet->fontSansSerif);
    }
}

bool CSnippetViewer::canClose()
{
    if (waitPanel->isVisible()) return false; // prevent closing while translation thread active
    return true;
}
