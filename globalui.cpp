#include <QApplication>
#include <QMimeData>
#include <QToolTip>
#include <QThread>
#include "globalui.h"
#include "structures.h"
#include "globalcontrol.h"
#include "auxtranslator.h"
#include "qxttooltip.h"
#include "specwidgets.h"
#include "genericfuncs.h"

namespace CDefaults {
const int globalStatusTooltipShowDelay = 100;
const int globalStatusTooltipShowDuration = 3000;
const int globalTranslatorDelay = 1000;
}

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
    actionTMAdditive->setData(CStructures::tmAdditive);

    actionTMOverwriting = new QAction(tr("Overwriting"),this);
    actionTMOverwriting->setCheckable(true);
    actionTMOverwriting->setActionGroup(translationMode);
    actionTMOverwriting->setData(CStructures::tmOverwriting);

    actionTMTooltip = new QAction(tr("Tooltip"),this);
    actionTMTooltip->setCheckable(true);
    actionTMTooltip->setActionGroup(translationMode);
    actionTMTooltip->setData(CStructures::tmTooltip);

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

    gctxTimer.setInterval(CDefaults::globalTranslatorDelay);
    gctxTimer.setSingleShot(true);
    gctxSelection.clear();
    connect(&gctxTimer,&QTimer::timeout,
            this,&CGlobalUI::startGlobalContextTranslate);

    gctxTranHotkey.setDisabled();
    connect(&gctxTranHotkey,&QxtGlobalShortcut::activated,
            actionGlobalTranslator,&QAction::toggle);
}

bool CGlobalUI::useOverrideFont() const
{
    return actionOverrideFont->isChecked();
}

bool CGlobalUI::autoTranslate() const
{
    return actionAutoTranslate->isChecked();
}

bool CGlobalUI::forceFontColor() const
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
    CLangPair lp(gSet->ui()->getActiveLangPair());
    if (!lp.isValid()) return;

    auto th = new QThread();
    auto at = new CAuxTranslator();
    at->setText(gctxSelection);
    at->setSrcLang(lp.langFrom.bcp47Name());
    at->setDestLang(lp.langTo.bcp47Name());
    connect(th,&QThread::finished,at,&CAuxTranslator::deleteLater);
    connect(th,&QThread::finished,th,&QThread::deleteLater);
    connect(at,&CAuxTranslator::gotTranslation,
            this,&CGlobalUI::gctxTranslateReady,Qt::QueuedConnection);
    at->moveToThread(th);
    th->start();

    QMetaObject::invokeMethod(at,&CAuxTranslator::translateAndQuit,Qt::QueuedConnection);

    gctxSelection.clear();
}

bool CGlobalUI::translateSubSentences() const
{
    return actionTranslateSubSentences->isChecked();
}

void CGlobalUI::addActionNotification(QAction *action)
{
    connect(action,&QAction::toggled,this,&CGlobalUI::actionToggled);
}

void CGlobalUI::gctxTranslateReady(const QString &text)
{
    const int maxTooltipCharacterWidth = 80;

    CSpecToolTipLabel* t = new CSpecToolTipLabel(CGenericFuncs::wordWrap(text,maxTooltipCharacterWidth));
    t->setStyleSheet(QStringLiteral("QLabel { background: #fefdeb; color: black; }"));
    QPoint p = QCursor::pos();
    QxtToolTip::show(p,t,nullptr);
}

void CGlobalUI::showGlobalTooltip(const QString &text)
{
    const int globalStatusTooltipFontSizeFrac = 125;
    const QPoint globalStatusTooltipOffset(90,90);

    if (gSet==nullptr || gSet->activeWindow()==nullptr) return;

    int sz = globalStatusTooltipFontSizeFrac * gSet->app()->font().pointSize()/100;

    QString msg = QStringLiteral("<span style='font-size:%1pt;white-space:nowrap;'>"
                                         "%2</span>").arg(sz).arg(text);
    QPoint pos = gSet->activeWindow()->mapToGlobal(globalStatusTooltipOffset);

    QTimer::singleShot(CDefaults::globalStatusTooltipShowDelay,gSet,[msg,pos](){
        if (gSet->activeWindow()==nullptr) return;
        QToolTip::showText(pos,msg,gSet->activeWindow());

        QTimer::singleShot(CDefaults::globalStatusTooltipShowDuration,gSet,[](){
            if (gSet->activeWindow())
                QToolTip::showText(QPoint(0,0),QString(),gSet->activeWindow());
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


    for (const CLangPair& pair : qAsConst(g->settings()->translatorPairs)) {
        QAction *ac = languageSelector->addAction(QStringLiteral("%1 - %2").arg(
                                      g->getLanguageName(pair.langFrom.bcp47Name()),
                                      g->getLanguageName(pair.langTo.bcp47Name())));
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

    Q_EMIT g->updateAllLanguagesLists();
}

void CGlobalUI::actionToggled()
{
    auto ac = qobject_cast<QAction *>(sender());
    if (ac==nullptr) return;

    QString msg = ac->text();
    msg.remove('&');
    if (ac->isCheckable()) {
        if (ac->isChecked()) {
            msg.append(tr(" is ON"));
        } else {
            msg.append(tr(" is OFF"));
        }
    }

    showGlobalTooltip(msg);
}

CStructures::TranslationMode CGlobalUI::getTranslationMode() const
{
    bool okconv;
    CStructures::TranslationMode res = CStructures::tmAdditive;
    if (translationMode->checkedAction()) {
        res = static_cast<CStructures::TranslationMode>(translationMode->checkedAction()->data().toInt(&okconv));
        if (!okconv)
            res = CStructures::tmAdditive;
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
