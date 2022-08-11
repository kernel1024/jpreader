#ifndef GLOBALPRIVATE_H
#define GLOBALPRIVATE_H

#include <QObject>
#include <QTimer>
#include <QLocalServer>
#include <QMutex>
#include <QIcon>
#include <QFileSystemWatcher>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QNetworkAccessManager>
#include "structures.h"
#include "settings.h"
#include "browser-utils/adblockrule.h"
#include "browser-utils/userscript.h"
#include "browser-utils/downloadmanager.h"
#include "browser-utils/downloadwriter.h"
#include "browser-utils/autofillassistant.h"
#include "utils/auxdictionary.h"
#include "utils/cliworker.h"
#include "utils/workermonitor.h"
#include "translator/auxtranslator.h"
#include "translator/uitranslator.h"
#include "translator/lighttranslator.h"
#include "zdict/zdictcontroller.h"

class CMainWindow;
class CLightTranslator;
class CAuxTranslator;
class CDownloadManager;
class BookmarksManager;
class CLogDisplay;
class CBrowserController;
class CTranslatorCache;
class CAbstractThreadWorker;
class CWorkerMonitor;
class CZipWriter;

class CGlobalControlPrivate : public QObject
{
    Q_OBJECT
public:
    CMainWindow* activeWindow { nullptr };
    QVector<CMainWindow*> mainWindows;
    QWebEngineProfile *webProfile { nullptr };
    QNetworkAccessManager *auxNetManager { nullptr };
    BookmarksManager *bookmarksManager { nullptr };
    ZDict::ZDictController * dictManager { nullptr };
    CTranslatorCache *translatorCache { nullptr };
    CZipWriter *zipWriter { nullptr };

    QWebEngineProfile *domWorkerProfile { nullptr };

    QScopedPointer<CLogDisplay, QScopedPointerDeleteLater> logWindow;
    QScopedPointer<CDownloadManager, QScopedPointerDeleteLater> downloadManager;
    QScopedPointer<CAuxDictionary, QScopedPointerDeleteLater> auxDictionary;
    QScopedPointer<QLocalServer, QScopedPointerDeleteLater> ipcServer;
    QScopedPointer<CLightTranslator, QScopedPointerDeleteLater> lightTranslator;
    QScopedPointer<CUITranslator, QScopedPointerDeleteLater> uiTranslator;
    QScopedPointer<CCLIWorker, QScopedPointerDeleteLater> cliWorker;
    QScopedPointer<CWorkerMonitor, QScopedPointerDeleteLater> workerMonitor;
    QScopedPointer<CAutofillAssistant, QScopedPointerDeleteLater> autofillAssistant;
    QScopedPointer<QFileSystemWatcher, QScopedPointerDeleteLater> xapianFilesystemWatcher;
    QScopedPointer<QWebEngineView, QScopedPointerDeleteLater> domWorker;
    QList<CAbstractThreadWorker *> workerPool;
    QMutex domWorkerMutex;

    QStringList recentFiles;
    CStringHash ctxSearchEngines;
    CUrlHolderVector recycleBin;
    CUrlHolderVector mainHistory;
    QStringList searchHistory;
    QHash<QString,QIcon> favicons;
    QStringList createdFiles;
    QStringList pixivKeywordsHistory;
    CStringHash pixivCommonCovers;

    bool blockTabCloseActive { false };

    CAuxTranslator* auxTranslatorDBus { nullptr };
    CBrowserController* browserControllerDBus { nullptr };

    QTimer tabsListTimer;
    QIcon m_appIcon;
    bool restoreLoadChecked { false };

    int mangaDetectedScrollDelta { CDefaults::mangaScrollDelta };
    qint64 mangaAvgFineRenderTime { 0L };
    QMutex mangaFineRenderMutex;
    QList<qint64> mangaFineRenderTimes;

    QHash<QString, CUserScript> userScripts;
    QMutex userScriptsMutex;
    bool cleaningState { false };
    bool sslCertErrorInteractive { false };

    CAdBlockVector adblock;
    QMutex adblockModifyMutex;
    QStringList adblockWhiteList;
    QMutex adblockWhiteListMutex;

    CStringSet noScriptWhiteList;
    QHash<QString,QSet<QString> > pageScriptHosts;
    QMutex noScriptMutex;

    QMap<QString,QString> langSortedBCP47List;  // names -> bcp (for sorting)
    QHash<QString,QString> langNamesList;       // bcp -> names

    QTimer settingsSaveTimer;
    QMutex settingsSaveMutex;

    QTimer xapianIndexerTimer;
    QMutex xapianTimerMutex;
    QStringList xapianIndexerPathAccumulator;

    QMutex pixivCommonCoversMutex;

    QSize openFileDialogSize;
    QSize saveFileDialogSize;
    QStringList debugMessages;

    explicit CGlobalControlPrivate(QObject *parent = nullptr);
    ~CGlobalControlPrivate() override;

    static void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static bool runnedFromQtCreator();

    void clearAdblockWhiteList();
    void reloadXapianFilesystemWatcher(QObject *control = nullptr);

    int getMaxUserInotifyWatches();

private:
    Q_DISABLE_COPY(CGlobalControlPrivate)

};

#endif // GLOBALPRIVATE_H
