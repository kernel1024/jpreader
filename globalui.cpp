#include <QApplication>
#include <QMimeData>
#include <QToolTip>
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
    addActionNotification(actionGlobalTranslator);

    actionSelectionDictionary = new QAction(tr("Dictionary search"),this);
    actionSelectionDictionary->setShortcut(QKeySequence(Qt::Key_F9));
    actionSelectionDictionary->setCheckable(true);
    actionSelectionDictionary->setChecked(false);
    addActionNotification(actionSelectionDictionary);

    actionUseProxy = new QAction(tr("Use proxy"),this);
    actionUseProxy->setCheckable(true);
    actionUseProxy->setChecked(false);

    actionJSUsage = new QAction(tr("Enable JavaScript"),this);
    actionJSUsage->setCheckable(true);
    actionJSUsage->setChecked(false);

    actionSnippetAutotranslate = new QAction(tr("Autotranslate snippet text"),this);
    actionSnippetAutotranslate->setCheckable(true);
    actionSnippetAutotranslate->setChecked(false);

    languageSelector = new QActionGroup(this);

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

    actionAutoTranslate = new QAction(QIcon::fromTheme(QStringLiteral("document-edit-decrypt")),
                                      tr("Automatic translation"),this);
    actionAutoTranslate->setCheckable(true);
    actionAutoTranslate->setChecked(false);
    actionAutoTranslate->setShortcut(Qt::Key_F8);
    addActionNotification(actionAutoTranslate);

    actionOverrideFont = new QAction(QIcon::fromTheme(QStringLiteral("character-set")),
                                     tr("Override font for translated text"),this);
    actionOverrideFont->setCheckable(true);
    actionOverrideFont->setChecked(false);

    actionOverrideFontColor = new QAction(QIcon::fromTheme(QStringLiteral("format-text-color")),
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
    auto th = new QThread();
    auto at = new CAuxTranslator();
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

void CGlobalUI::addActionNotification(QAction *action)
{
    connect(action,&QAction::toggled,this,&CGlobalUI::actionToggled);
}

void CGlobalUI::gctxTranslateReady(const QString &text)
{
    CSpecToolTipLabel* t = new CSpecToolTipLabel(wordWrap(text,80));
    t->setStyleSheet(QStringLiteral("QLabel { background: #fefdeb; color: black; }"));
    QPoint p = QCursor::pos();
    QxtToolTip::show(p,t,nullptr);
}

CMainWindow* CGlobalUI::addMainWindow()
{
     return addMainWindowEx(false, true);
}

CMainWindow* CGlobalUI::addMainWindowEx(bool withSearch, bool withViewer, const QVector<QUrl>& viewerUrls)
{
    if (gSet==nullptr) return nullptr;

    auto mainWindow = new CMainWindow(withSearch,withViewer,viewerUrls);
    connect(mainWindow,&CMainWindow::aboutToClose,
            gSet,&CGlobalControl::windowDestroyed,Qt::QueuedConnection);

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
    mainWindow->menuSettings->addAction(actionOverrideFont);
    mainWindow->menuSettings->addAction(actionOverrideFontColor);
    mainWindow->menuSettings->addSeparator();
    mainWindow->menuSettings->addAction(actionUseProxy);
    mainWindow->menuSettings->addAction(actionTranslateSubSentences);

    mainWindow->menuTranslationMode->addAction(actionTMAdditive);
    mainWindow->menuTranslationMode->addAction(actionTMOverwriting);
    mainWindow->menuTranslationMode->addAction(actionTMTooltip);

    gSet->settings.checkRestoreLoad(mainWindow);

    connect(gSet,&CGlobalControl::updateAllBookmarks,mainWindow,&CMainWindow::updateBookmarks);
    connect(gSet,&CGlobalControl::updateAllRecentLists,mainWindow,&CMainWindow::updateRecentList);
    connect(gSet,&CGlobalControl::updateAllCharsetLists,mainWindow,&CMainWindow::reloadCharsetList);
    connect(gSet,&CGlobalControl::updateAllHistoryLists,mainWindow,&CMainWindow::updateHistoryList);
    connect(gSet,&CGlobalControl::updateAllQueryLists,mainWindow,&CMainWindow::updateQueryHistory);
    connect(gSet,&CGlobalControl::updateAllRecycleBins,mainWindow,&CMainWindow::updateRecycled);
    connect(gSet,&CGlobalControl::updateAllLanguagesLists,mainWindow,&CMainWindow::reloadLanguagesList);

    return mainWindow;
}

void CGlobalUI::showGlobalTooltip(const QString &text)
{
    if (gSet->activeWindow==nullptr) return;

    int sz = 5*qApp->font().pointSize()/4;

    QString msg = QStringLiteral("<span style='font-size:%1pt;white-space:nowrap;'>"
                                         "%2</span>").arg(sz).arg(text);
    QPoint pos = gSet->activeWindow->mapToGlobal(QPoint(90,90));

    QTimer::singleShot(100,gSet,[msg,pos](){
        if (gSet->activeWindow==nullptr) return;
        QToolTip::showText(pos,msg,gSet->activeWindow);

        QTimer::singleShot(3000,gSet,[](){
            if (gSet->activeWindow)
                QToolTip::showText(QPoint(0,0),QString(),gSet->activeWindow);
        });
    });
}

void CGlobalUI::rebuildLanguageActions(QObject * control)
{
    QObject* cg = control;
    if (cg==nullptr) cg = gSet;
    if (cg==nullptr) return;

    auto g = qobject_cast<CGlobalControl *>(cg);
    if (g==nullptr) return;

    QString selectedHash;
    bool first = (languageSelector->checkedAction()==nullptr);
    if (!first)
        selectedHash = languageSelector->checkedAction()->data().toString();

    languageSelector->deleteLater();
    languageSelector = new QActionGroup(this);


    for (const CLangPair& pair : qAsConst(g->settings.translatorPairs)) {
        QAction *ac = languageSelector->addAction(QStringLiteral("%1 - %2").arg(
                                      g->getLanguageName(pair.getLangFrom().bcp47Name()),
                                      g->getLanguageName(pair.getLangTo().bcp47Name())));
        ac->setCheckable(true);
        QString hash = pair.getHash();
        ac->setData(hash);

        if (first || hash==selectedHash) {
            ac->setChecked(true);
            first = false;
        }
    }

    if (languageSelector->actions().isEmpty()) {
        QAction *ac = languageSelector->addAction(QStringLiteral("(empty)"));
        ac->setEnabled(false);
    }

    emit g->updateAllLanguagesLists();
}

void CGlobalUI::actionToggled()
{
    auto ac = qobject_cast<QAction *>(sender());
    if (ac==nullptr) return;

    QString msg = ac->text();
    msg.remove('&');
    if (ac->isCheckable()) {
        if (ac->isChecked())
            msg.append(tr(" is ON"));
        else
            msg.append(tr(" is OFF"));
    }

    showGlobalTooltip(msg);
}

int CGlobalUI::getTranslationMode()
{
    bool okconv;
    int res = 0;
    if (translationMode->checkedAction()) {
        res = translationMode->checkedAction()->data().toInt(&okconv);
        if (!okconv)
            res = 0;
    }
    return res;
}

QString CGlobalUI::getActiveLangPair() const
{
    QString res;
    if (languageSelector->checkedAction()) {
        res = languageSelector->checkedAction()->data().toString();
    } else if (!languageSelector->actions().isEmpty()) {
        const QList<QAction *> acl = languageSelector->actions();
        QAction* ac = acl.first();
        if (ac->isEnabled())
            res = ac->data().toString();
    }
    return res;
}
