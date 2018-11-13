#include <QShortcut>
#include <QUrl>
#include <QFileInfo>
#include <QTimer>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QCompleter>
#include <QUrlQuery>
#include <QDrag>
#include <QMimeData>
#include <QPrinter>
#include <QPageSetupDialog>

#include <math.h>
#include "snviewer.h"
#include "mainwindow.h"
#include "specwidgets.h"
#include "globalcontrol.h"
#include "genericfuncs.h"

#include "snctxhandler.h"
#include "snnet.h"
#include "sntrans.h"
#include "snmsghandler.h"

CSnippetViewer::CSnippetViewer(CMainWindow* parent, QUrl aUri, QStringList aSearchText, bool setFocused,
                               QString AuxContent, QString zoom, bool startPage)
    : CSpecTabContainer(parent)
{
	setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);

    tabTitle.clear();
    isStartPage = startPage;
    translationBkgdFinished=false;
    loadingBkgdFinished=false;
    loading = false;
    fileChanged = false;
    calculatedUrl.clear();
    onceTranslated=false;
    requestAutotranslate=false;
    pageLoaded=false;
    auxContentLoaded=false;
    pageImage = QPixmap();
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

    connect(backButton, &QPushButton::clicked, msgHandler, &CSnMsgHandler::searchBack);
    connect(fwdButton, &QPushButton::clicked, msgHandler, &CSnMsgHandler::searchFwd);
    connect(stopButton, &QPushButton::clicked, txtBrowser, &QWebEngineView::stop);
    connect(reloadButton, &QPushButton::clicked, txtBrowser, &QWebEngineView::reload);
    connect(urlEdit, &QLineEdit::returnPressed, this, &CSnippetViewer::navByUrlDefault);
    connect(navButton, &QPushButton::clicked, msgHandler, &CSnMsgHandler::navByClick);
    connect(fwdNavButton, &QPushButton::clicked, txtBrowser, &QWebEngineView::forward);
    connect(backNavButton, &QPushButton::clicked, txtBrowser, &QWebEngineView::back);
    connect(comboZoom, &QComboBox::currentTextChanged, msgHandler, &CSnMsgHandler::setZoom);
    connect(searchEdit->lineEdit(), &QLineEdit::returnPressed, fwdButton, &QPushButton::click);
    connect(passwordButton, &QPushButton::clicked, msgHandler, &CSnMsgHandler::pastePassword);

    connect(txtBrowser, &QWebEngineView::loadProgress, transHandler, &CSnTrans::progressLoad);
    connect(txtBrowser, &QWebEngineView::loadFinished, netHandler, &CSnNet::loadFinished);
    connect(txtBrowser, &QWebEngineView::loadStarted, netHandler, &CSnNet::loadStarted);
    connect(txtBrowser, &QWebEngineView::titleChanged, this, &CSnippetViewer::titleChanged);
    connect(txtBrowser, &QWebEngineView::urlChanged, this, &CSnippetViewer::urlChanged);
    connect(txtBrowser, &QWebEngineView::iconChanged, this, &CSnippetViewer::updateTabIcon);

    connect(txtBrowser, &QWebEngineView::renderProcessTerminated,
            msgHandler, &CSnMsgHandler::renderProcessTerminated);

    connect(txtBrowser->page(), &QWebEnginePage::authenticationRequired,
            netHandler, &CSnNet::authenticationRequired);
    connect(txtBrowser->page(), &QWebEnginePage::proxyAuthenticationRequired,
            netHandler, &CSnNet::proxyAuthenticationRequired);

    connect(msgHandler->loadingBarHideTimer, &QTimer::timeout, barLoading, &QProgressBar::hide);
    connect(msgHandler->loadingBarHideTimer, &QTimer::timeout, barPlaceholder, &QFrame::show);

    connect(txtBrowser->page(), &QWebEnginePage::linkHovered,
            msgHandler, &CSnMsgHandler::linkHovered);
    connect(txtBrowser->customPage(), &CSpecWebPage::linkClickedExt,
            netHandler, &CSnNet::userNavigationRequest);

    backNavButton->setIcon(QIcon::fromTheme("go-previous"));
    fwdNavButton->setIcon(QIcon::fromTheme("go-next"));
    backButton->setIcon(QIcon::fromTheme("go-up-search"));
	fwdButton->setIcon(QIcon::fromTheme("go-down-search"));
	reloadButton->setIcon(QIcon::fromTheme("view-refresh"));
    stopButton->setIcon(QIcon::fromTheme("process-stop"));
    navButton->setIcon(QIcon::fromTheme("arrow-right"));
    passwordButton->setIcon(QIcon::fromTheme("dialog-password"));

    for (int i=0;i<TECOUNT;i++)
        comboTranEngine->addItem(gSet->getTranslationEngineString(i),i);
    comboTranEngine->setCurrentIndex(gSet->settings.translatorEngine);
    connect(comboTranEngine, SIGNAL(currentIndexChanged(int)), msgHandler, SLOT(tranEngine(int)));
    connect(&(gSet->settings), &CSettings::settingsUpdated, msgHandler, &CSnMsgHandler::updateTranEngine);

    QShortcut* sc;
    sc = new QShortcut(QKeySequence(Qt::Key_Slash),this);
    connect(sc, &QShortcut::activated, msgHandler, &CSnMsgHandler::searchFocus);
    sc = new QShortcut(QKeySequence(Qt::Key_F + Qt::CTRL),this);
    connect(sc, &QShortcut::activated, msgHandler, &CSnMsgHandler::searchFocus);

    waitPanel->hide();
    errorPanel->hide();
    errorLabel->setText(QString());
    searchPanel->setVisible(!searchList.isEmpty());

    if (AuxContent.isEmpty())
        netHandler->load(aUri);
    else
        netHandler->load(AuxContent);

    int idx = comboZoom->findText(zoom,Qt::MatchExactly);
    if (idx>=0) {
        comboZoom->setCurrentIndex(idx);
    } else {
        comboZoom->addItem(zoom);
        comboZoom->setCurrentIndex(comboZoom->findText(zoom,Qt::MatchExactly));
    }

    int mv = (70*parentWnd->height()/(urlEdit->fontMetrics().height()*100));
    completer->setMaxVisibleItems(mv);

    ctxHandler->reconfigureDefaultActions();

    if (!startPage)
        parentWnd->closeStartPage();

    msgHandler->focusTimer->start();
}

void CSnippetViewer::updateButtonsState()
{
    stopButton->setEnabled(loading);
    reloadButton->setEnabled(!loading);
    fwdNavButton->setEnabled(txtBrowser->history()->canGoForward());
    backNavButton->setEnabled(txtBrowser->history()->canGoBack());
    passwordButton->setEnabled((!loading) &&
                               (txtBrowser->page()!=nullptr) &&
                               (gSet->haveSavedPassword(txtBrowser->page()->url())));
}

void CSnippetViewer::navByUrlDefault()
{
    navByUrl(urlEdit->text());
}

void CSnippetViewer::navByUrl(QUrl url)
{
    urlEdit->setStyleSheet(QString());

    fileChanged=false;

    netHandler->load(url);
}

void CSnippetViewer::navByUrl(QString url)
{
    QString aUrl = url;
    QUrl u = QUrl::fromUserInput(aUrl);
    QStringList validSpecSchemes;
    validSpecSchemes << "gdlookup" << "about" << "chrome";

    if (!validSpecSchemes.contains(u.scheme().toLower())
            && (!u.isValid() || !url.contains('.'))
            && !gSet->ctxSearchEngines.isEmpty())
        u = gSet->createSearchUrl(url);

    navByUrl(u);
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
        if (s.isEmpty()) s = QLatin1String("< ... >");
	}
    s = s.trimmed();
    if (!title.isEmpty())
        tabTitle = title.trimmed();
    else
        tabTitle = s;

    parentWnd->titleRenamedLock.start();
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
        else if (netHandler->isValidLoadedUrl())
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

void CSnippetViewer::outsideDragStart()
{
    QDrag* drag = new QDrag(this);
    QMimeData* mime = new QMimeData();

    QString s = QString("%1\n%2").arg(getUrl().toString(),tabTitle);
    mime->setData("_NETSCAPE_URL",s.toUtf8());
    drag->setMimeData(mime);

    drag->exec(Qt::MoveAction);
}

bool CSnippetViewer::isInspector()
{
    QUrl u = gSet->getInspectorUrl();
    QUrl uc = txtBrowser->url();
    u.setFragment(QString());
    u.setQuery(QString());
    u.setPath(QString());
    uc.setFragment(QString());
    uc.setQuery(QString());
    uc.setPath(QString());

    return (u==uc);
}

void CSnippetViewer::printToPDF()
{
    QPrinter printer;
    QPageSetupDialog dlg(&printer,this);
    if (dlg.exec() != QDialog::Accepted)
            return;

    QString fname = getSaveFileNameD(this,tr("Save to PDF"),gSet->settings.savedAuxSaveDir,
                                                 tr("PDF file (*.pdf)"));
    if (fname.isEmpty()) return;
    gSet->settings.savedAuxSaveDir = QFileInfo(fname).absolutePath();

    txtBrowser->page()->printToPdf(fname, printer.pageLayout());
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
                gSet->webProfile->settings()->testAttribute(
                    QWebEngineSettings::AutoLoadImages));

    if (gSet->settings.overrideStdFonts) {
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::StandardFont,gSet->settings.fontStandard);
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::FixedFont,gSet->settings.fontFixed);
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::SerifFont,gSet->settings.fontSerif);
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::SansSerifFont,gSet->settings.fontSansSerif);
    }
}

bool CSnippetViewer::canClose()
{
    if (waitPanel->isVisible()) return false; // prevent closing while translation thread active
    return true;
}

void CSnippetViewer::takeScreenshot()
{
    if (!pageLoaded) return;
    QTimer* t = new QTimer(this);
    t->setInterval(500);
    t->setSingleShot(true);
    connect(t,&QTimer::timeout,[this,t](){
        pageImage = txtBrowser->grab();
        t->deleteLater();
    });
    t->start();
}
