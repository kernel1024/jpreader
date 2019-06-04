#ifndef GLOBALPRIVATE_H
#define GLOBALPRIVATE_H

#include <QObject>
#include <QTimer>
#include <QLocalServer>
#include <QMutex>
#include <QIcon>
#include "structures.h"
#include "adblockrule.h"
#include "userscript.h"
#include "auxdictionary.h"
#include "auxtranslator.h"

class CMainWindow;
class CLightTranslator;
class CAuxTranslator;
class ArticleNetworkAccessManager;
class CGoldenDictMgr;
class CDownloadManager;
class BookmarksManager;
class CLogDisplay;
class CBrowserController;
class CSettingsDlg;

const int defaultInspectorPort = 50000;

class CGlobalControlPrivate : public QObject
{
    Q_OBJECT
public:
    CMainWindow* activeWindow { nullptr };
    QVector<CMainWindow*> mainWindows;
    CLightTranslator* lightTranslator { nullptr };
    CLogDisplay* logWindow { nullptr };
    CDownloadManager* downloadManager { nullptr };
    QWebEngineProfile *webProfile { nullptr };
    QNetworkAccessManager *auxNetManager { nullptr };
    BookmarksManager *bookmarksManager { nullptr };
    ArticleNetworkAccessManager * dictNetMan { nullptr };
    CGoldenDictMgr * dictManager { nullptr };

    QStringList recentFiles;
    CStringHash ctxSearchEngines;
    CUrlHolderVector recycleBin;
    CUrlHolderVector mainHistory;
    QStringList searchHistory;
    QHash<QString,QIcon> favicons;
    QStringList createdFiles;

    int inspectorPort { defaultInspectorPort };
    bool blockTabCloseActive { false };

    QLocalServer* ipcServer { nullptr };
    CAuxDictionary* auxDictionary { nullptr };
    CAuxTranslator* auxTranslatorDBus { nullptr };
    CBrowserController* browserControllerDBus { nullptr };

    QTimer tabsListTimer;
    QIcon m_appIcon;
    bool restoreLoadChecked { false };

    QHash<QString, CUserScript> userScripts;
    QMutex userScriptsMutex;
    bool cleaningState { false };
    bool atlCertErrorInteractive { false };

    CAdBlockVector adblock;
    QStringList adblockWhiteList;
    QMutex adblockWhiteListMutex;

    CStringSet noScriptWhiteList;
    QHash<QString,QSet<QString> > pageScriptHosts;
    QMutex noScriptMutex;

    QMap<QString,QString> langSortedBCP47List;  // names -> bcp (for sorting)
    QHash<QString,QString> langNamesList;       // bcp -> names

    QTimer settingsSaveTimer;
    QMutex settingsSaveMutex;
    CSettingsDlg* settingsDialog { nullptr };
    QSize settingsDlgSize;


public:
    explicit CGlobalControlPrivate(QObject *parent = nullptr);

Q_SIGNALS:

public Q_SLOTS:
    void settingsDialogDestroyed();
};

#endif // GLOBALPRIVATE_H
