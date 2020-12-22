#include <QShortcut>
#include <QUrl>
#include <QFileInfo>
#include <QTimer>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QCompleter>
#include <QUrlQuery>
#include <QDrag>
#include <QMimeData>
#include <QPrinter>
#include <QPageSetupDialog>
#include <QAuthenticator>

#include <cmath>
#include "browser.h"
#include "mainwindow.h"
#include "utils/specwidgets.h"
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/structures.h"
#include "global/contentfiltering.h"
#include "global/browserfuncs.h"
#include "global/network.h"
#include "global/history.h"
#include "global/ui.h"

#include "ctxhandler.h"
#include "net.h"
#include "trans.h"
#include "waitctl.h"
#include "msghandler.h"

namespace CDefaults {
const int tabPreviewScreenshotDelay = 500;
}

CBrowserTab::CBrowserTab(QWidget *parent, const QUrl& aUri, const QStringList& aSearchText,
                         bool setFocused, const QString& AuxContent, const QString& zoom,
                         bool startPage)

    : CSpecTabContainer(parent)
{
    const int addressBarCompleterHeightFrac = 70;

    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);

    m_startPage = startPage;
    barLoading->setValue(0);
    barLoading->hide();
    barPlaceholder->show();
    if (!aSearchText.isEmpty()) m_searchList.append(aSearchText);
    if (m_searchList.count()>0) {
        searchEdit->addItems(aSearchText);
        searchEdit->setCurrentIndex(0);
    }
    bindToTab(parentWnd()->tabMain, setFocused);

    netHandler = new CBrowserNet(this);
    ctxHandler = new CBrowserCtxHandler(this);
    transHandler = new CBrowserTrans(this);
    msgHandler = new CBrowserMsgHandler(this);
    waitHandler = new CBrowserWaitCtl(this);

    auto *completer = new QCompleter(this);
    completer->setModel(new CSpecUrlHistoryModel(completer));
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    urlEdit->setCompleter(completer);

    connect(backButton, &QPushButton::clicked, msgHandler, &CBrowserMsgHandler::searchBack);
    connect(fwdButton, &QPushButton::clicked, msgHandler, &CBrowserMsgHandler::searchFwd);
    connect(stopButton, &QPushButton::clicked, txtBrowser, &QWebEngineView::stop);
    connect(urlEdit, &QLineEdit::returnPressed, this, &CBrowserTab::navByUrlDefault);
    connect(navButton, &QPushButton::clicked, msgHandler, &CBrowserMsgHandler::navByClick);
    connect(fwdNavButton, &QPushButton::clicked, txtBrowser, &QWebEngineView::forward);
    connect(backNavButton, &QPushButton::clicked, txtBrowser, &QWebEngineView::back);
    connect(comboZoom, &QComboBox::currentTextChanged, msgHandler, &CBrowserMsgHandler::setZoom);
    connect(searchEdit->lineEdit(), &QLineEdit::returnPressed, fwdButton, &QPushButton::click);
    connect(passwordButton, &QPushButton::clicked, msgHandler, &CBrowserMsgHandler::pastePassword);
    connect(reloadButton, &QPushButton::clicked, txtBrowser, [this](){
        txtBrowser->pageAction(QWebEnginePage::ReloadAndBypassCache)->activate(QAction::Trigger);
    });

    connect(txtBrowser, &QWebEngineView::loadProgress, netHandler, &CBrowserNet::progressLoad);
    connect(txtBrowser, &QWebEngineView::loadFinished, netHandler, &CBrowserNet::loadFinished);
    connect(txtBrowser, &QWebEngineView::loadStarted, netHandler, &CBrowserNet::loadStarted);
    connect(txtBrowser, &QWebEngineView::titleChanged, this, &CBrowserTab::titleChanged);
    connect(txtBrowser, &QWebEngineView::urlChanged, this, &CBrowserTab::urlChanged);
    connect(txtBrowser, &QWebEngineView::iconChanged, this, &CBrowserTab::updateTabIcon);

    connect(txtBrowser, &QWebEngineView::renderProcessTerminated,
            msgHandler, &CBrowserMsgHandler::renderProcessTerminated);

    connect(txtBrowser->page(), &QWebEnginePage::authenticationRequired,
            netHandler, &CBrowserNet::authenticationRequired);
    connect(txtBrowser->page(), &QWebEnginePage::proxyAuthenticationRequired,
            netHandler, &CBrowserNet::proxyAuthenticationRequired);

    connect(msgHandler, &CBrowserMsgHandler::loadingBarHide, barLoading, &QProgressBar::hide);
    connect(msgHandler, &CBrowserMsgHandler::loadingBarHide, barPlaceholder, &QFrame::show);

    connect(txtBrowser->page(), &QWebEnginePage::linkHovered,
            msgHandler, &CBrowserMsgHandler::linkHovered);
    connect(txtBrowser->customPage(), &CSpecWebPage::linkClickedExt,
            netHandler, &CBrowserNet::userNavigationRequest);

    backNavButton->setIcon(QIcon::fromTheme(QSL("go-previous")));
    fwdNavButton->setIcon(QIcon::fromTheme(QSL("go-next")));
    backButton->setIcon(QIcon::fromTheme(QSL("go-up-search")));
    fwdButton->setIcon(QIcon::fromTheme(QSL("go-down-search")));
    reloadButton->setIcon(QIcon::fromTheme(QSL("view-refresh")));
    stopButton->setIcon(QIcon::fromTheme(QSL("process-stop")));
    navButton->setIcon(QIcon::fromTheme(QSL("arrow-right")));
    passwordButton->setIcon(QIcon::fromTheme(QSL("dialog-password")));

    for (auto it = CStructures::translationEngines().constKeyValueBegin(),
         end = CStructures::translationEngines().constKeyValueEnd(); it != end; ++it) {
        comboTranEngine->addItem((*it).second,(*it).first);
    }
    int idx = comboTranEngine->findData(gSet->settings()->translatorEngine);
    if (idx>=0)
        comboTranEngine->setCurrentIndex(idx);

    connect(comboTranEngine, qOverload<int>(&QComboBox::currentIndexChanged),
            msgHandler, &CBrowserMsgHandler::tranEngine);
    comboTranEngine->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(comboTranEngine, &QComboBox::customContextMenuRequested,
            msgHandler, &CBrowserMsgHandler::languageContextMenu);
    connect(gSet->ui(), &CGlobalUI::translationEngineChanged, msgHandler, &CBrowserMsgHandler::updateTranEngine);

    QShortcut* sc = nullptr;
    sc = new QShortcut(QKeySequence(Qt::Key_Slash),this);
    connect(sc, &QShortcut::activated, msgHandler, &CBrowserMsgHandler::searchFocus);
    sc = new QShortcut(QKeySequence(Qt::Key_F + Qt::CTRL),this);
    connect(sc, &QShortcut::activated, msgHandler, &CBrowserMsgHandler::searchFocus);
    sc = new QShortcut(QKeySequence(Qt::Key_F12),this);
    connect(sc, &QShortcut::activated, msgHandler, &CBrowserMsgHandler::showInspector);

    waitPanel->hide();
    errorPanel->hide();
    errorLabel->setText(QString());
    searchPanel->setVisible(!m_searchList.isEmpty());

    splitter->setChildrenCollapsible(true);
    QList<int> heights({ 1, 0 });
    splitter->setSizes(heights);

    if (AuxContent.isEmpty()) {
        netHandler->load(aUri);
    } else {
        netHandler->load(AuxContent);
    }

    idx = comboZoom->findText(zoom,Qt::MatchExactly);
    if (idx>=0) {
        comboZoom->setCurrentIndex(idx);
    } else {
        comboZoom->addItem(zoom);
        comboZoom->setCurrentIndex(comboZoom->findText(zoom,Qt::MatchExactly));
    }

    int mv = (addressBarCompleterHeightFrac*parentWnd()->height() /
              (urlEdit->fontMetrics().height()*100));
    completer->setMaxVisibleItems(mv);

    ctxHandler->reconfigureDefaultActions();

    if (!startPage)
        parentWnd()->closeStartPage();

    msgHandler->activateFocusDelay();
}

void CBrowserTab::setRequestAutotranslate(bool requestAutotranslate)
{
    m_requestAutotranslate = requestAutotranslate;
}

void CBrowserTab::setRequestAlternateAutotranslate(bool requestAlternateAutotranslate)
{
    m_requestAlternateAutotranslate = requestAlternateAutotranslate;
}

void CBrowserTab::setTranslationRestriction()
{
    m_onceTranslated = true;
}

void CBrowserTab::updateTabColor(bool loadFinished, bool tranFinished)
{
    int selfIdx = parentWnd()->tabMain->indexOf(this);
    if (selfIdx<0) return;

    // reset color on focus acquiring
    if (!loadFinished && !tranFinished) {
        parentWnd()->tabMain->tabBar()->setTabTextColor(
                    selfIdx,gSet->app()->palette(parentWnd()->tabMain->tabBar()).windowText().color());
        return;
    }

    QColor color;

    // change color for background tab only
    if (parentWnd()->tabMain->currentIndex()!=selfIdx) {
        if (loadFinished && !m_translationBkgdFinished) {
            m_loadingBkgdFinished=true;
            color = QColor(Qt::blue);

        } else if (tranFinished && !m_loadingBkgdFinished) {
            m_translationBkgdFinished=true;
            if (m_fileChanged)
                color = QColor(Qt::red);
        }

        if (color.isValid()) {
            parentWnd()->tabMain->tabBar()->setTabTextColor(selfIdx,color);
            parentWnd()->updateTabs();
        }
    }
}

void CBrowserTab::updateButtonsState()
{
    stopButton->setEnabled(m_loading);
    reloadButton->setEnabled(!m_loading);
    fwdNavButton->setEnabled(txtBrowser->history()->canGoForward());
    backNavButton->setEnabled(txtBrowser->history()->canGoBack());
    passwordButton->setEnabled((!m_loading) &&
                               (txtBrowser->page()!=nullptr) &&
                               (gSet->browser()->haveSavedPassword(txtBrowser->page()->url(),QString())));
}

void CBrowserTab::navByUrlDefault()
{
    navByUrl(urlEdit->text());
}

void CBrowserTab::navByUrl(const QUrl& url)
{
    urlEdit->setStyleSheet(QString());

    m_fileChanged=false;

    netHandler->load(url);
}

void CBrowserTab::navByUrl(const QString& url)
{
    QUrl u = QUrl::fromUserInput(url);
    static const QStringList validSpecSchemes
            = { QSL("gdlookup"), QSL("about"), QSL("chrome"), CMagicFileSchemeHandler::getScheme() };

    if (!validSpecSchemes.contains(u.scheme().toLower())
            && (!u.isValid() || !url.contains(u'.'))) {
        u = gSet->net()->createSearchUrl(url);
    }

    navByUrl(u);
}

void CBrowserTab::titleChanged(const QString & title)
{
    QString s = title;
    QUrl uri;
    if (s.isEmpty()) {
        uri = getUrl();
        s=uri.toLocalFile();
        if (!s.isEmpty()) {
            s=QFileInfo(s).fileName();
        } else {
            QStringList sl = uri.path().split(u'/');
            if (!sl.isEmpty()) {
                s=sl.last();
            } else {
                s=uri.toString();
            }
        }
        if (s.isEmpty()) s = QSL("< ... >");
        s = s.trimmed();
    } else {
        s = title.trimmed();
    }

    setTabTitle(s);

    if (!title.isEmpty()) {
        if (txtBrowser->page()->url().isValid() &&
                !txtBrowser->page()->url().toString().contains(
                    QSL("about:blank"),Qt::CaseInsensitive)) {
            CUrlHolder uh(title,txtBrowser->page()->url());
            if (!gSet->history()->updateMainHistoryTitle(uh,title.trimmed()))
                gSet->history()->appendMainHistory(uh);
        }
    }
    parentWnd()->updateTitle();
    parentWnd()->updateTabs();
}

void CBrowserTab::urlChanged(const QUrl & url)
{
    if (url.scheme().startsWith(QSL("http"),Qt::CaseInsensitive) ||
            url.scheme().startsWith(QSL("file"),Qt::CaseInsensitive) ||
            url.scheme().startsWith(CMagicFileSchemeHandler::getScheme(),Qt::CaseInsensitive)) {
        urlEdit->setText(url.toString());

    } else {
        QUrl aUrl = getUrl();
        if (url.scheme().startsWith(QSL("http"),Qt::CaseInsensitive) ||
                url.scheme().startsWith(QSL("file"),Qt::CaseInsensitive) ||
                url.scheme().startsWith(CMagicFileSchemeHandler::getScheme(),Qt::CaseInsensitive)) {
            urlEdit->setText(aUrl.toString());

        } else if (netHandler->isValidLoadedUrl()) {
            urlEdit->setText(netHandler->getLoadedUrl().toString());

        } else {
            urlEdit->clear();
        }
    }

    if (m_fileChanged) {
        urlEdit->setToolTip(tr("File changed. Temporary copy loaded in memory."));
        urlEdit->setStyleSheet(QSL("QLineEdit { background: #d7ffd7; }"));
    } else {
        urlEdit->setToolTip(tr("Displayed URL"));
        urlEdit->setStyleSheet(QString());
    }
}

void CBrowserTab::recycleTab()
{
    if (tabTitle().isEmpty() || !txtBrowser->page()->url().isValid() ||
            txtBrowser->page()->url().toString().
            startsWith(QSL("about"),Qt::CaseInsensitive)) {
        return;
    }

    gSet->history()->appendRecycled(tabTitle(),txtBrowser->page()->url());
}

QUrl CBrowserTab::getUrl()
{
    return txtBrowser->url();
}

void CBrowserTab::setToolbarVisibility(bool visible)
{
    toolBar->setVisible(visible);
}

void CBrowserTab::outsideDragStart()
{
    auto *drag = new QDrag(this);
    auto *mime = new QMimeData();

    QString s = QSL("%1\n%2").arg(getUrl().toString(),tabTitle());
    mime->setData(QSL("_NETSCAPE_URL"),s.toUtf8());
    drag->setMimeData(mime);

    drag->exec(Qt::MoveAction);
}

void CBrowserTab::printToPDF()
{
    QPrinter printer;
    QPageSetupDialog dlg(&printer,this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString fname = CGenericFuncs::getSaveFileNameD(this,tr("Save to PDF"),gSet->settings()->savedAuxSaveDir,
                                                    QStringList( { tr("PDF file (*.pdf)") } ));
    if (fname.isEmpty()) return;
    gSet->ui()->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

    txtBrowser->page()->printToPdf(fname, printer.pageLayout());
}

void CBrowserTab::statusBarMsg(const QString &msg)
{
    parentWnd()->statusBar()->showMessage(msg);
}

QString CBrowserTab::getDocTitle()
{
    QString s = txtBrowser->title();
    if (s.isEmpty()) {
        QStringList f = getUrl().path().split(u'/');
        if (!f.isEmpty()) {
            s = f.last();
        } else {
            s = getUrl().toString();
        }
    }
    return s;
}

void CBrowserTab::keyPressEvent(QKeyEvent *event)
{
    if (event->key()==Qt::Key_F5) {
        closeTab();

    } else if (event->key()==Qt::Key_F3) {
        msgHandler->searchFwd();

    } else if (event->key()==Qt::Key_F2) {
        msgHandler->searchBack();

    } else {
        QWidget::keyPressEvent(event);
    }
}

void CBrowserTab::updateWebViewAttributes()
{
    txtBrowser->settings()->setAttribute(
                QWebEngineSettings::AutoLoadImages,
                gSet->browser()->webProfile()->settings()->testAttribute(
                    QWebEngineSettings::AutoLoadImages));

    if (gSet->settings()->overrideStdFonts) {
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::StandardFont,
                                              gSet->settings()->fontStandard);
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::FixedFont,
                                              gSet->settings()->fontFixed);
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::SerifFont,
                                              gSet->settings()->fontSerif);
        txtBrowser->settings()->setFontFamily(QWebEngineSettings::SansSerifFont,
                                              gSet->settings()->fontSansSerif);
    }
    if (gSet->settings()->overrideFontSizes) {
        txtBrowser->settings()->setFontSize(QWebEngineSettings::MinimumFontSize,
                                            gSet->settings()->fontSizeMinimal);
        txtBrowser->settings()->setFontSize(QWebEngineSettings::DefaultFontSize,
                                            gSet->settings()->fontSizeDefault);
        txtBrowser->settings()->setFontSize(QWebEngineSettings::DefaultFixedFontSize,
                                            gSet->settings()->fontSizeFixed);
    }
}

bool CBrowserTab::canClose()
{
    // cleanup some temporary data
    gSet->contentFilter()->clearNoScriptPageHosts(getUrl().toString(CSettings::adblockUrlFmt));

    return !waitPanel->isVisible(); // prevent closing while translation thread active
}

void CBrowserTab::takeScreenshot()
{
    if (!m_pageLoaded) return;
    QTimer::singleShot(CDefaults::tabPreviewScreenshotDelay,this,[this](){
        m_pageImage = txtBrowser->grab();
    });
}

void CBrowserTab::save()
{
    ctxHandler->saveToFile();
}

void CBrowserTab::tabAcquiresFocus()
{
    updateTabColor();
    m_translationBkgdFinished=false;
    m_loadingBkgdFinished=false;
    txtBrowser->setFocus(Qt::OtherFocusReason);
    takeScreenshot();
}
