#include "search/xapianindexworker.h"

#include <QApplication>
#include <QActionGroup>
#include <QMimeData>
#include <QToolTip>
#include <QThread>

#include "actions.h"
#include "structures.h"
#include "control.h"
#include "startup.h"
#include "ui.h"
#include "network.h"
#include "history.h"
#include "browser-utils/autofillassistant.h"
#include "translator/auxtranslator.h"
#include "miniqxt/qxttooltip.h"
#include "utils/specwidgets.h"
#include "utils/genericfuncs.h"

namespace CDefaults {
const int globalStatusTooltipShowDelay = 100;
const int globalStatusTooltipShowDuration = 3000;
const int globalTranslatorDelay = 1000;
const int globalThreadWorkerTestInterval = 5000;
const auto subsentencesActionGroupID = "ACG_ENGINE_ID";
}

CGlobalActions::CGlobalActions(QObject *parent)
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

    actionXapianForceFullScan = new QAction(tr("Force full scan"),this);
    actionXapianClearAndRescan = new QAction(QIcon::fromTheme(QSL("data-warning")),
                                             tr("Clear index and full rescan"),this);
    actionXapianClearAndRescan->setData(QSL("cleanup"));

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

    actionAutoTranslate = new QAction(QIcon::fromTheme(QSL("document-edit-decrypt")),
                                      tr("Automatic translation"),this);
    actionAutoTranslate->setCheckable(true);
    actionAutoTranslate->setChecked(false);
    actionAutoTranslate->setShortcut(Qt::Key_F8);
    addActionNotification(actionAutoTranslate);

    actionOverrideTransFont = new QAction(QIcon::fromTheme(QSL("character-set")),
                                     tr("Override font for translated text"),this);
    actionOverrideTransFont->setCheckable(true);
    actionOverrideTransFont->setChecked(false);

    actionOverrideTransFontColor = new QAction(QIcon::fromTheme(QSL("format-text-color")),
                                          tr("Force translated text color"),this);
    actionOverrideTransFontColor->setCheckable(true);
    actionOverrideTransFontColor->setChecked(false);

    actionLogNetRequests = new QAction(tr("Log network requests"),this);
    actionLogNetRequests->setCheckable(true);
    actionLogNetRequests->setChecked(false);

    connect(QApplication::clipboard(),&QClipboard::changed,
            this,&CGlobalActions::clipboardChanged);

    gctxTimer.setInterval(CDefaults::globalTranslatorDelay);
    gctxTimer.setSingleShot(true);
    gctxSelection.clear();
    connect(&gctxTimer,&QTimer::timeout,
            this,&CGlobalActions::startGlobalContextTranslate);

    threadedWorkerTestTimer.setInterval(CDefaults::globalThreadWorkerTestInterval);
    threadedWorkerTestTimer.setSingleShot(false);
    connect(&threadedWorkerTestTimer,&QTimer::timeout,
            this,&CGlobalActions::updateBusyCursor);
    threadedWorkerTestTimer.start();
}

bool CGlobalActions::useOverrideTransFont() const
{
    return actionOverrideTransFont->isChecked();
}

bool CGlobalActions::autoTranslate() const
{
    return actionAutoTranslate->isChecked();
}

bool CGlobalActions::forceFontColor() const
{
    return actionOverrideTransFontColor->isChecked();
}

void CGlobalActions::clipboardChanged(QClipboard::Mode mode)
{
    QClipboard *cb = QApplication::clipboard();

    if (actionGlobalTranslator->isChecked() &&
        (mode==QClipboard::Selection) &&
        (!cb->mimeData(QClipboard::Selection)->text().isEmpty())) {
        gctxSelection = cb->mimeData(QClipboard::Selection)->text();
        gctxTimer.start();
    }
}

void CGlobalActions::startGlobalContextTranslate()
{
    if (gctxSelection.isEmpty()) return;
    CLangPair lp(gSet->actions()->getActiveLangPair());
    if (!lp.isValid()) return;

    auto *th = new QThread();
    auto *at = new CAuxTranslator();
    at->setText(gctxSelection);
    at->setSrcLang(lp.langFrom.bcp47Name());
    at->setDestLang(lp.langTo.bcp47Name());
    connect(th,&QThread::finished,at,&CAuxTranslator::deleteLater);
    connect(th,&QThread::finished,th,&QThread::deleteLater);
    connect(at,&CAuxTranslator::gotTranslation,
            this,&CGlobalActions::gctxTranslateReady,Qt::QueuedConnection);
    at->moveToThread(th);
    th->setObjectName(QSL("GCTX"));
    th->start();

    QMetaObject::invokeMethod(at,&CAuxTranslator::translateAndQuit,Qt::QueuedConnection);

    gctxSelection.clear();
}

void CGlobalActions::addActionNotification(QAction *action) const
{
    connect(action,&QAction::toggled,this,&CGlobalActions::actionToggled);
}

void CGlobalActions::rebindGctxHotkey(QObject *control)
{
    QObject* cg = control;
    if (cg==nullptr) cg = gSet;
    if (cg==nullptr) return;

    auto *g = qobject_cast<CGlobalControl *>(cg);
    Q_ASSERT(g!=nullptr);

    if (gctxTranHotkey)
        gctxTranHotkey->deleteLater();

    if (!(g->settings()->gctxSequence.isEmpty())) {
        gctxTranHotkey = new QxtGlobalShortcut(g->settings()->gctxSequence,this);
        connect(gctxTranHotkey.data(), &QxtGlobalShortcut::activated, actionGlobalTranslator, &QAction::toggle);
    }
}

void CGlobalActions::gctxTranslateReady(const QString &text)
{
    const int maxTooltipCharacterWidth = 80;

    auto *t = new QLabel(CGenericFuncs::wordWrap(text,maxTooltipCharacterWidth));
    t->setStyleSheet(QSL("QLabel { background: #fefdeb; color: black; }"));
    QPoint p = QCursor::pos();
    QxtToolTip::show(p,t,nullptr,QRect(),false,true);
}

void CGlobalActions::showGlobalTooltip(const QString &text)
{
    const int globalStatusTooltipFontSizeFrac = 125;
    const QPoint globalStatusTooltipOffset(90,90);

    if (gSet==nullptr || gSet->activeWindow()==nullptr) return;

    int sz = globalStatusTooltipFontSizeFrac * gSet->app()->font().pointSize()/100;

    QString msg = QSL("<span style='font-size:%1pt;white-space:nowrap;'>"
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

void CGlobalActions::rebuildLanguageActions(QObject * control)
{
    QObject* cg = control;
    if (cg==nullptr) cg = gSet;
    if (cg==nullptr) return;

    auto *g = qobject_cast<CGlobalControl *>(cg);
    Q_ASSERT(g!=nullptr);

    QString selectedHash;
    bool firstRun = (languageSelector->checkedAction()==nullptr);
    if (firstRun) {
        selectedHash = g->m_settings->selectedLangPairs.value(g->m_settings->translatorEngine,QString());
    } else {
        selectedHash = languageSelector->checkedAction()->data().toString();
    }

    languageSelector->deleteLater();
    languageSelector = new QActionGroup(this);

    connect(languageSelector,&QActionGroup::triggered,g,[](QAction* action){
        gSet->m_settings->selectedLangPairs[gSet->m_settings->translatorEngine] = action->data().toString();
    });

    for (const CLangPair& pair : qAsConst(g->settings()->translatorPairs)) {
        QAction *ac = languageSelector->addAction(QSL("%1 - %2").arg(
                                      g->m_net->getLanguageName(pair.langFrom.bcp47Name()),
                                      g->m_net->getLanguageName(pair.langTo.bcp47Name())));
        ac->setCheckable(true);
        QString hash = pair.getHash();
        ac->setData(hash);

        if ((firstRun && selectedHash.isEmpty()) ||
                (firstRun && !selectedHash.isEmpty() && hash==selectedHash) ||
                (!firstRun && hash==selectedHash)) {
            ac->setChecked(true);
            firstRun = false;
        }
    }

    if (languageSelector->actions().isEmpty()) {
        QAction *ac = languageSelector->addAction(QSL("(empty)"));
        ac->setEnabled(false);
    }
    updateSubsentencesModeActions(getSubsentencesModeHash());

    Q_EMIT g->m_history->updateAllLanguagesLists();
}

void CGlobalActions::updateSubsentencesModeActions(const CSubsentencesMode &hash)
{
    for (const auto &submenu : qAsConst(subsentencesMode))
        submenu->deleteLater();
    subsentencesMode.clear();

    for (auto it = CStructures::translationEngines().constBegin(),
         end = CStructures::translationEngines().constEnd(); it != end; ++it) {
        auto *acg = new QActionGroup(this);
        acg->setProperty(CDefaults::subsentencesActionGroupID,QVariant::fromValue(it.key()));
        acg->setExclusionPolicy(QActionGroup::ExclusionPolicy::Exclusive);

        QAction *ac = acg->addAction(tr("Split by punctuation"));
        ac->setCheckable(true);
        ac->setChecked((hash.value(it.key()) == CStructures::smSplitByPunctuation));
        ac->setData(QVariant::fromValue(CStructures::smSplitByPunctuation));

        ac = acg->addAction(tr("Keep paragraph"));
        ac->setCheckable(true);
        ac->setChecked((hash.value(it.key()) == CStructures::smKeepParagraph));
        ac->setData(QVariant::fromValue(CStructures::smKeepParagraph));

        ac = acg->addAction(tr("Force combine"));
        ac->setCheckable(true);
        ac->setChecked((hash.value(it.key()) == CStructures::smCombineToMaxTokens));
        ac->setData(QVariant::fromValue(CStructures::smCombineToMaxTokens));

        subsentencesMode[it.key()] = acg;
    }
}

CStructures::SubsentencesMode CGlobalActions::getSubsentencesMode(CStructures::TranslationEngine engine) const
{
    const auto list = subsentencesMode.value(engine);

    if (list.isNull())
        return CStructures::SubsentencesMode::smKeepParagraph;

    for (auto * const ac : (list->actions())) {
        if ((ac != nullptr) && (ac->isChecked()))
            return ac->data().value<CStructures::SubsentencesMode>();
    }

    return CStructures::SubsentencesMode::smKeepParagraph;
}

CSubsentencesMode CGlobalActions::getSubsentencesModeHash() const
{
    CSubsentencesMode res;
    for (auto it = subsentencesMode.constKeyValueBegin(); it != subsentencesMode.constKeyValueEnd(); ++ it)
        res[it->first] = getSubsentencesMode(it->first);

    return res;
}

void CGlobalActions::setSubsentencesModeHash(const CSubsentencesMode &hash)
{
    updateSubsentencesModeActions(hash);
}

bool CGlobalActions::eventFilter(QObject *object, QEvent *event)
{
    auto *g = qobject_cast<CGlobalControl *>(parent());
    if (g == nullptr)
        return QObject::eventFilter(object, event);

    Qt::Key sc_key = Qt::Key(0); // NOLINT
    Qt::KeyboardModifiers sc_mods = Qt::NoModifier;

    if (!g->m_settings->autofillSequence.isEmpty()) {
// TODO: remove all Qt 5.x checks!

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        sc_key = g->m_settings->autofillSequence[0].key();
        sc_mods = g->m_settings->autofillSequence[0].keyboardModifiers();
#else
        Qt::KeyboardModifiers allMods = Qt::ShiftModifier | Qt::ControlModifier
                                        | Qt::AltModifier | Qt::MetaModifier;

        auto sc = static_cast<unsigned int>(g->m_settings->autofillSequence[0]);
        sc_key = static_cast<Qt::Key>((sc ^ allMods) & sc);
        sc_mods = Qt::KeyboardModifiers(sc & allMods);
#endif
    }

    if ((event->type() == QEvent::KeyPress) && (sc_key != Qt::Key(0))) {
        auto * const keyEvent = dynamic_cast<QKeyEvent*>(event);
        const int key = keyEvent->key();
        const Qt::KeyboardModifiers mods = keyEvent->modifiers();

        if ((sc_key == key) && (sc_mods == mods)) {
            if (g->autofillAssistant())
                g->autofillAssistant()->pasteAndForward();
            return true;
        }
    }

    return QObject::eventFilter(object, event);
}

void CGlobalActions::actionToggled()
{
    auto *ac = qobject_cast<QAction *>(sender());
    if (ac==nullptr) return;

    QString msg = ac->text();
    msg.remove(u'&');
    if (ac->isCheckable()) {
        if (ac->isChecked()) {
            msg.append(tr(" is ON"));
        } else {
            msg.append(tr(" is OFF"));
        }
    }

    showGlobalTooltip(msg);
}

void CGlobalActions::updateBusyCursor()
{
    static QAtomicInteger<bool> busyActive(false); // initially normal cursor

    if (gSet->startup()->isThreadedWorkersActive()) {
        if (busyActive.testAndSetOrdered(false,true))
            gSet->app()->setOverrideCursor(Qt::BusyCursor);
    } else {
        if (busyActive.testAndSetOrdered(true,false))
            gSet->app()->restoreOverrideCursor();
    }
}

CStructures::TranslationMode CGlobalActions::getTranslationMode() const
{
    bool okconv = false;
    CStructures::TranslationMode res = CStructures::tmAdditive;
    if (translationMode->checkedAction()) {
        res = static_cast<CStructures::TranslationMode>(translationMode->checkedAction()->data().toInt(&okconv));
        if (!okconv)
            res = CStructures::tmAdditive;
    }
    return res;
}

QString CGlobalActions::getActiveLangPair() const
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

void CGlobalActions::setActiveLangPair(const QString &hash) const
{
    for (auto& ac : languageSelector->actions()) {
        if (ac->data().toString() == hash) {
            ac->setChecked(true);
            break;
        }
    }
}

QList<QAction *> CGlobalActions::getTranslationLanguagesActions() const
{
    QList<QAction* > res;
    if (gSet->m_actions.isNull()) return res;

    return gSet->m_actions->languageSelector->actions();
}

QList<QAction *> CGlobalActions::getSubsentencesModeActions(CStructures::TranslationEngine engine) const
{
    QList<QAction* > res;
    if (gSet->m_actions.isNull()) return res;
    const auto ptr = gSet->m_actions->subsentencesMode.value(engine);
    if (ptr.isNull()) return res;

    return ptr->actions();
}
