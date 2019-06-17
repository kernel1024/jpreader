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

namespace CDefaults {
const int defaultInspectorPort = 50000;
}

class CGlobalControlPrivate : public QObject
{
    Q_OBJECT
public:
    CMainWindow* activeWindow { nullptr };
    QVector<CMainWindow*> mainWindows;
    QWebEngineProfile *webProfile { nullptr };
    QNetworkAccessManager *auxNetManager { nullptr };
    BookmarksManager *bookmarksManager { nullptr };
    ArticleNetworkAccessManager * dictNetMan { nullptr };
    CGoldenDictMgr * dictManager { nullptr };

    QScopedPointer<CLogDisplay> logWindow;
    QScopedPointer<CDownloadManager> downloadManager;
    QScopedPointer<CAuxDictionary> auxDictionary;
    QScopedPointer<QLocalServer> ipcServer;
    QScopedPointer<CLightTranslator> lightTranslator;

    QStringList recentFiles;
    CStringHash ctxSearchEngines;
    CUrlHolderVector recycleBin;
    CUrlHolderVector mainHistory;
    QStringList searchHistory;
    QHash<QString,QIcon> favicons;
    QStringList createdFiles;

    int inspectorPort { CDefaults::defaultInspectorPort };
    bool blockTabCloseActive { false };

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

    QSize openFileDialogSize;
    QSize saveFileDialogSize;
    QStringList debugMessages;

    explicit CGlobalControlPrivate(QObject *parent = nullptr);
    ~CGlobalControlPrivate() override;

    static void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static bool runnedFromQtCreator();

private:
    Q_DISABLE_COPY(CGlobalControlPrivate)

};

#endif // GLOBALPRIVATE_H