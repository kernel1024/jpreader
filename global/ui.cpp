#include <QTextCodec>
#include <QWebEngineSettings>

#include "ui.h"
#include "control.h"
#include "control_p.h"
#include "history.h"
#include "startup.h"
#include "browserfuncs.h"
#include "translator/lighttranslator.h"
#include "translator/translatorstatisticstab.h"

namespace CDefaults{
const int tabCloseInterlockDelay = 500;
}

CGlobalUI::CGlobalUI(QObject *parent)
    : QObject(parent)
{
}

QIcon CGlobalUI::appIcon() const
{
    return gSet->d_func()->m_appIcon;
}

void CGlobalUI::setSavedAuxSaveDir(const QString &dir)
{
    gSet->m_settings->savedAuxSaveDir = dir;
}

void CGlobalUI::setSavedAuxDir(const QString &dir)
{
    gSet->m_settings->savedAuxDir = dir;
}

void CGlobalUI::addMainWindow()
{
    addMainWindowEx(false, true);
}

CMainWindow* CGlobalUI::addMainWindowEx(bool withSearch, bool withBrowser, const QVector<QUrl>& viewerUrls) const
{
    if (gSet==nullptr || gSet->m_actions.isNull()) return nullptr;

    auto *mainWindow = new CMainWindow(withSearch,withBrowser,viewerUrls);
    connect(mainWindow,&CMainWindow::aboutToClose,
            this,&CGlobalUI::windowDestroyed,Qt::QueuedConnection);

    gSet->d_func()->mainWindows.append(mainWindow);

    mainWindow->show();

    mainWindow->menuTools->addAction(gSet->m_actions->actionLogNetRequests);
    mainWindow->menuTools->addSeparator();
    mainWindow->menuTools->addAction(gSet->m_actions->actionGlobalTranslator);
    mainWindow->menuTools->addAction(gSet->m_actions->actionSelectionDictionary);
    mainWindow->menuTools->addAction(gSet->m_actions->actionSnippetAutotranslate);
    mainWindow->menuTools->addSeparator();
    mainWindow->menuTools->addAction(gSet->m_actions->actionJSUsage);

    mainWindow->menuSettings->addAction(gSet->m_actions->actionAutoTranslate);
    mainWindow->menuSettings->addAction(gSet->m_actions->actionOverrideTransFont);
    mainWindow->menuSettings->addAction(gSet->m_actions->actionOverrideTransFontColor);
    mainWindow->menuSettings->addSeparator();
    mainWindow->menuSettings->addAction(gSet->m_actions->actionUseProxy);

    mainWindow->menuTranslationMode->addAction(gSet->m_actions->actionTMAdditive);
    mainWindow->menuTranslationMode->addAction(gSet->m_actions->actionTMOverwriting);
    mainWindow->menuTranslationMode->addAction(gSet->m_actions->actionTMTooltip);

    gSet->m_settings->checkRestoreLoad(mainWindow);

    connect(gSet->history(),&CGlobalHistory::updateAllBookmarks,mainWindow,&CMainWindow::updateBookmarks);
    connect(gSet->history(),&CGlobalHistory::updateAllRecentLists,mainWindow,&CMainWindow::updateRecentList);
    connect(gSet->history(),&CGlobalHistory::updateAllCharsetLists,mainWindow,&CMainWindow::reloadCharsetList);
    connect(gSet->history(),&CGlobalHistory::updateAllHistoryLists,mainWindow,&CMainWindow::updateHistoryList);
    connect(gSet->history(),&CGlobalHistory::updateAllQueryLists,mainWindow,&CMainWindow::updateQueryHistory);
    connect(gSet->history(),&CGlobalHistory::updateAllRecycleBins,mainWindow,&CMainWindow::updateRecycled);
    connect(gSet->history(),&CGlobalHistory::updateAllLanguagesLists,mainWindow,&CMainWindow::reloadLanguagesList);

    return mainWindow;
}

void CGlobalUI::settingsTab()
{
    gSet->m_settings->settingsTab();
}

void CGlobalUI::translationStatisticsTab()
{
    auto *dlg = CTranslatorStatisticsTab::instance();
    if (dlg!=nullptr) {
        dlg->activateWindow();
        dlg->setTabFocused();
    }
}

QRect CGlobalUI::getLastMainWindowGeometry() const
{
    QRect res;
    if (!gSet->d_func()->mainWindows.isEmpty())
        res = gSet->d_func()->mainWindows.constLast()->geometry();

    return res;
}

bool CGlobalUI::isBlockTabCloseActive() const
{
    return gSet->d_func()->blockTabCloseActive;
}

void CGlobalUI::setFileDialogNewFolderName(const QString &name)
{
    gSet->d_func()->uiTranslator->setFileDialogNewFolderName(name);
}

void CGlobalUI::setPixivFetchCovers(CStructures::PixivFetchCoversMode mode)
{
    gSet->m_settings->pixivFetchCovers = mode;
}

void CGlobalUI::blockTabClose()
{
    gSet->d_func()->blockTabCloseActive=true;
    QTimer::singleShot(CDefaults::tabCloseInterlockDelay,this,[](){
        gSet->d_func()->blockTabCloseActive=false;
    });
}

void CGlobalUI::showDictionaryWindow()
{
    showDictionaryWindowEx(QString());
}

void CGlobalUI::showDictionaryWindowEx(const QString &text)
{
    if (gSet->d_func()->auxDictionary.isNull()) {
        gSet->d_func()->auxDictionary.reset(new CAuxDictionary());
        gSet->d_func()->auxDictionary->show();
        gSet->d_func()->auxDictionary->adjustSplitters();
    }

    if (!text.isEmpty()) {
        gSet->d_func()->auxDictionary->findWord(text);
    } else {
        gSet->d_func()->auxDictionary->restoreWindow();
    }
}

void CGlobalUI::focusChanged(QWidget *old, QWidget *now)
{
    Q_UNUSED(old)

    if (now==nullptr) return;
    auto *mw = qobject_cast<CMainWindow*>(now->window());
    if (mw==nullptr) return;
    gSet->d_func()->activeWindow=mw;
}

void CGlobalUI::windowDestroyed(CMainWindow *obj)
{
    if (obj==nullptr) return;
    gSet->d_func()->mainWindows.removeOne(obj);
    if (gSet->d_func()->activeWindow==obj) {
        if (gSet->d_func()->mainWindows.count()>0) {
            gSet->d_func()->activeWindow = gSet->d_func()->mainWindows.constFirst();
            gSet->d_func()->activeWindow->activateWindow();
        } else {
            gSet->d_func()->activeWindow=nullptr;
            gSet->m_startup->cleanupAndExit();
        }
    }
}

void CGlobalUI::showLightTranslator(const QString &text)
{
    if (gSet->d_func()->lightTranslator.isNull())
        gSet->d_func()->lightTranslator.reset(new CLightTranslator());

    gSet->d_func()->lightTranslator->restoreWindow();

    if (!text.isEmpty())
        gSet->d_func()->lightTranslator->appendSourceText(text);
}

void CGlobalUI::forceCharset()
{
    const int maxCharsetHistory = 10;

    auto *act = qobject_cast<QAction*>(sender());
    if (act==nullptr) return;

    QString cs = act->data().toString();
    if (!cs.isEmpty()) {
        QTextCodec* codec = QTextCodec::codecForName(cs.toLatin1().data());
        if (codec!=nullptr)
            cs = QString::fromUtf8(codec->name());

        gSet->m_settings->charsetHistory.removeAll(cs);
        gSet->m_settings->charsetHistory.prepend(cs);
        if (gSet->m_settings->charsetHistory.count()>maxCharsetHistory)
            gSet->m_settings->charsetHistory.removeLast();
    }

    gSet->m_settings->forcedCharset = cs;
    Q_EMIT gSet->history()->updateAllCharsetLists();

    if (gSet->m_browser->webProfile()!=nullptr && gSet->m_browser->webProfile()->settings()!=nullptr) {
        gSet->m_browser->webProfile()->settings()->setDefaultTextEncoding(cs);
    }
}
