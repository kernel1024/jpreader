#include <QApplication>
#include <QMimeData>
#include "globalui.h"
#include "structures.h"
#include "globalcontrol.h"
#include "auxtranslator.h"
#include "qxttooltip.h"
#include "specwidgets.h"
#include "genericfuncs.h"

CGlobalUI::CGlobalUI(QObject *parent)
    : QObject(parent)
{
    actionGlobalTranslator = new QAction(tr("Global context translator"),this);
    actionGlobalTranslator->setCheckable(true);
    actionGlobalTranslator->setChecked(false);

    actionSelectionDictionary = new QAction(tr("Dictionary search"),this);
    actionSelectionDictionary->setShortcut(QKeySequence(Qt::Key_F9));
    actionSelectionDictionary->setCheckable(true);
    actionSelectionDictionary->setChecked(false);

    actionUseProxy = new QAction(tr("Use proxy"),this);
    actionUseProxy->setCheckable(true);
    actionUseProxy->setChecked(false);

    actionJSUsage = new QAction(tr("Enable JavaScript"),this);
    actionJSUsage->setCheckable(true);
    actionJSUsage->setChecked(false);

    actionSnippetAutotranslate = new QAction(tr("Autotranslate snippet text"),this);
    actionSnippetAutotranslate->setCheckable(true);
    actionSnippetAutotranslate->setChecked(false);

    translationMode = new QActionGroup(this);

    actionTMAdditive = new QAction(tr("Additive"),this);
    actionTMAdditive->setCheckable(true);
    actionTMAdditive->setActionGroup(translationMode);
    actionTMAdditive->setData(TM_ADDITIVE);

    actionTMOverwriting = new QAction(tr("Overwriting"),this);
    actionTMOverwriting->setCheckable(true);
    actionTMOverwriting->setActionGroup(translationMode);
    actionTMOverwriting->setData(TM_OVERWRITING);

    actionTMTooltip = new QAction(tr("Tooltip"),this);
    actionTMTooltip->setCheckable(true);
    actionTMTooltip->setActionGroup(translationMode);
    actionTMTooltip->setData(TM_TOOLTIP);

    actionTMAdditive->setChecked(true);

    sourceLanguage = new QActionGroup(this);

    actionLSJapanese = new QAction(tr("Japanese"),this);
    actionLSJapanese->setCheckable(true);
    actionLSJapanese->setActionGroup(sourceLanguage);
    actionLSJapanese->setData(LS_JAPANESE);

    actionLSChineseSimplified = new QAction(tr("Chinese simplified"),this);
    actionLSChineseSimplified->setCheckable(true);
    actionLSChineseSimplified->setActionGroup(sourceLanguage);
    actionLSChineseSimplified->setData(LS_CHINESESIMP);

    actionLSChineseTraditional = new QAction(tr("Chinese traditional"),this);
    actionLSChineseTraditional->setCheckable(true);
    actionLSChineseTraditional->setActionGroup(sourceLanguage);
    actionLSChineseTraditional->setData(LS_CHINESETRAD);

    actionLSKorean = new QAction(tr("Korean"),this);
    actionLSKorean->setCheckable(true);
    actionLSKorean->setActionGroup(sourceLanguage);
    actionLSKorean->setData(LS_KOREAN);

    actionLSJapanese->setChecked(true);

    actionAutoTranslate = new QAction(QIcon::fromTheme("document-edit-decrypt"),
                                      tr("Automatic translation"),this);
    actionAutoTranslate->setCheckable(true);
    actionAutoTranslate->setChecked(false);
    actionAutoTranslate->setShortcut(Qt::Key_F8);

    actionOverrideFont = new QAction(QIcon::fromTheme("character-set"),
                                     tr("Override font for translated text"),this);
    actionOverrideFont->setCheckable(true);
    actionOverrideFont->setChecked(false);

    actionAutoloadImages = new QAction(QIcon::fromTheme("view-preview"),
                                       tr("Autoload images"),this);
    actionAutoloadImages->setCheckable(true);
    actionAutoloadImages->setChecked(false);

    actionOverrideFontColor = new QAction(QIcon::fromTheme("format-text-color"),
                                          tr("Force translated text color"),this);
    actionOverrideFontColor->setCheckable(true);
    actionOverrideFontColor->setChecked(false);

    actionLogNetRequests = new QAction(tr("Log network requests"),this);
    actionLogNetRequests->setCheckable(true);
    actionLogNetRequests->setChecked(false);

    actionTranslateSubSentences = new QAction(tr("Translate subsentences"),this);
    actionTranslateSubSentences->setCheckable(true);
    actionTranslateSubSentences->setChecked(false);

    connect(QApplication::clipboard(),&QClipboard::changed,
            this,&CGlobalUI::clipboardChanged);

    gctxTimer.setInterval(1000);
    gctxTimer.setSingleShot(true);
    gctxSelection.clear();
    connect(&gctxTimer,&QTimer::timeout,
            this,&CGlobalUI::startGlobalContextTranslate);

    gctxTranHotkey.setDisabled();
    connect(&gctxTranHotkey,&QxtGlobalShortcut::activated,
            actionGlobalTranslator,&QAction::toggle);
}

bool CGlobalUI::useOverrideFont()
{
    return actionOverrideFont->isChecked();
}

bool CGlobalUI::autoTranslate()
{
    return actionAutoTranslate->isChecked();
}

bool CGlobalUI::forceFontColor()
{
    return actionOverrideFontColor->isChecked();
}

void CGlobalUI::clipboardChanged(QClipboard::Mode mode)
{
    QClipboard *cb = QApplication::clipboard();

    if (actionGlobalTranslator->isChecked() &&
        (mode==QClipboard::Selection) &&
        (!cb->mimeData(QClipboard::Selection)->text().isEmpty())) {
        gctxSelection = cb->mimeData(QClipboard::Selection)->text();
        gctxTimer.start();
    }
}

void CGlobalUI::startGlobalContextTranslate()
{
    if (gctxSelection.isEmpty()) return;
    QThread *th = new QThread();
    CAuxTranslator *at = new CAuxTranslator();
    connect(this,&CGlobalUI::gctxStart,
            at,&CAuxTranslator::startAuxTranslation,Qt::QueuedConnection);
    connect(at,&CAuxTranslator::gotTranslation,
            this,&CGlobalUI::gctxTranslateReady,Qt::QueuedConnection);
    at->moveToThread(th);
    th->start();

    emit gctxStart(gctxSelection);

    gctxSelection.clear();
}

bool CGlobalUI::translateSubSentences()
{
    return actionTranslateSubSentences->isChecked();
}

void CGlobalUI::gctxTranslateReady(const QString &text)
{
    CSpecToolTipLabel* t = new CSpecToolTipLabel(wordWrap(text,80));
    t->setStyleSheet("QLabel { background: #fefdeb; color: black; }");
    QPoint p = QCursor::pos();
    QxtToolTip::show(p,t,NULL);
}

CMainWindow* CGlobalUI::addMainWindow(bool withSearch, bool withViewer, const QUrl& withViewerUrl)
{
    if (gSet==NULL) return NULL;

    CMainWindow* mainWindow = new CMainWindow(withSearch,withViewer,withViewerUrl);
    connect(mainWindow,&CMainWindow::aboutToClose,
            gSet,&CGlobalControl::windowDestroyed);

    gSet->mainWindows.append(mainWindow);

    mainWindow->show();

    mainWindow->menuTools->addAction(actionLogNetRequests);
    mainWindow->menuTools->addSeparator();
    mainWindow->menuTools->addAction(actionGlobalTranslator);
    mainWindow->menuTools->addAction(actionSelectionDictionary);
    mainWindow->menuTools->addAction(actionSnippetAutotranslate);
    mainWindow->menuTools->addSeparator();
    mainWindow->menuTools->addAction(actionJSUsage);

    mainWindow->menuSettings->addAction(actionAutoTranslate);
    mainWindow->menuSettings->addAction(actionAutoloadImages);
    mainWindow->menuSettings->addAction(actionOverrideFont);
    mainWindow->menuSettings->addAction(actionOverrideFontColor);
    mainWindow->menuSettings->addSeparator();
    mainWindow->menuSettings->addAction(actionUseProxy);
    mainWindow->menuSettings->addAction(actionTranslateSubSentences);

    mainWindow->menuTranslationMode->addAction(actionTMAdditive);
    mainWindow->menuTranslationMode->addAction(actionTMOverwriting);
    mainWindow->menuTranslationMode->addAction(actionTMTooltip);

    mainWindow->menuSourceLanguage->addAction(actionLSJapanese);
    mainWindow->menuSourceLanguage->addAction(actionLSChineseSimplified);
    mainWindow->menuSourceLanguage->addAction(actionLSChineseTraditional);
    mainWindow->menuSourceLanguage->addAction(actionLSKorean);

    gSet->settings.checkRestoreLoad(mainWindow);

    return mainWindow;
}
