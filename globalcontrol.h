#ifndef QGLOBALSETTINGS_H
#define QGLOBALSETTINGS_H

#include <QObject>
#include <QWidget>
#include <QApplication>
#include <QString>
#include <QUrl>
#include <QThread>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QAuthenticator>
#include <QMutex>
#include <QTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QWebEngineProfile>
#include <QNetworkAccessManager>
#include <QSslCertificate>
#include <QDebug>

#include "specwidgets.h"
#include "logdisplay.h"
#include "adblockrule.h"
#include "structures.h"
#include "settings.h"
#include "globalui.h"
#include "userscript.h"
#include "browsercontroller.h"
#include "bookmarks.h"

class CMainWindow;
class CLightTranslator;
class CAuxTranslator;
class CTranslator;
class CAuxDictionary;
class ArticleNetworkAccessManager;
class CGoldenDictMgr;
class CDownloadManager;
class BookmarksManager;

class CGlobalControl : public QObject
{
    friend class CSettings;

    Q_OBJECT
public:
    explicit CGlobalControl(QApplication *parent, int aInspectorPort);
    virtual ~CGlobalControl();

    CSettings settings;
    CGlobalUI ui;

    CMainWindow* activeWindow;
    QVector<CMainWindow*> mainWindows;
    CLightTranslator* lightTranslator;
    CAuxTranslator* auxTranslatorDBus;
    CBrowserController* browserControllerDBus;
    QLocalServer* ipcServer;

    QIcon appIcon;
    CLogDisplay* logWindow;
    CDownloadManager* downloadManager;

    ArticleNetworkAccessManager * dictNetMan;
    CGoldenDictMgr * dictManager;
    CAuxDictionary* auxDictionary;

    QWebEngineProfile *webProfile;
    QNetworkAccessManager *auxNetManager;

    BookmarksManager *bookmarksManager;

    QStringList recentFiles;

    CStringHash ctxSearchEngines;

    CUrlHolderVector recycleBin;
    CUrlHolderVector mainHistory;
    QStringList searchHistory;

    QHash<QString,QIcon> favicons;

    CAdBlockVector adblock;

    QStringList createdFiles;

    QTimer tabsListTimer;

    CSslCertificateHash atlCerts;

    int inspectorPort;
    bool blockTabCloseActive;

    // History lists append
    void appendRecycled(const QString &title, const QUrl &url);
    void appendSearchHistory(const QStringList &req);
    void appendMainHistory(const CUrlHolder &item);
    bool updateMainHistoryTitle(const CUrlHolder &item, const QString &newTitle);
    void appendRecent(const QString &filename);

    // Ad-block
    bool isUrlBlocked(const QUrl &url);
    bool isUrlBlocked(const QUrl &url, QString &filter);
    void adblockAppend(const QString &url);
    void adblockAppend(const CAdBlockRule &url);
    void adblockAppend(const QVector<CAdBlockRule> &urls);

    // No-Script
    bool isScriptBlocked(const QUrl &url, const QUrl &origin);
    void clearNoScriptPageHosts(const QString& origin);
    CStringSet getNoScriptPageHosts(const QString& origin);
    void removeNoScriptWhitelistHost(const QString& host);
    void addNoScriptWhitelistHost(const QString& host);
    bool containsNoScriptWhitelist(const QString& host) const;

    // Password management
    void readPassword(const QUrl &origin, QString &user, QString &password);
    void savePassword(const QUrl &origin, const QString &user, const QString &password);
    bool haveSavedPassword(const QUrl &origin);
    void removePassword(const QUrl &origin);
    QUrl cleanUrlForRealm(const QUrl &origin) const;

    // Userscripts
    QVector<CUserScript> getUserScriptsForUrl(const QUrl &url, bool isMainFrame, bool isContextMenu);
    void initUserScripts(const CStringHash& scripts);
    CStringHash getUserScripts();

    // Translation languages selection
    QStringList getLanguageCodes() const;
    QString getLanguageName(const QString &bcp47Name);
    QString getTranslationEngineString(int engine);
    void showLightTranslator(const QString& text = QString());

    // Misc
    QUrl createSearchUrl(const QString& text, const QString& engine = QString());
    QString makeTmpFileName(const QString &suffix, bool withDir = false);

    // Chromium Inspector console
    QUrl getInspectorUrl() const;

private:
    QHash<QString, CUserScript> userScripts;
    QMutex userScriptsMutex;
    bool cleaningState;
    bool atlCertErrorInteractive;

    QStringList adblockWhiteList;
    QMutex adblockWhiteListMutex;

    CStringSet noScriptWhiteList;
    QHash<QString,QSet<QString> > pageScriptHosts;
    QMutex noScriptMutex;

    QMap<QString,QString> langSortedBCP47List;  // names -> bcp (for sorting)
    QHash<QString,QString> langNamesList;       // bcp -> names

    bool setupIPC();
    void sendIPCMessage(QLocalSocket *socket, const QString& msg);
    void cleanTmpFiles();
    void initLanguagesList();

signals:
    void startAuxTranslation();
    void stopTranslators();

    void updateAllBookmarks();
    void updateAllRecycleBins();
    void updateAllCharsetLists();
    void updateAllQueryLists();
    void updateAllHistoryLists();
    void updateAllRecentLists();
    void updateAllLanguagesLists();

    void addAdBlockWhiteListUrl(const QString& url);
    void addNoScriptPageHost(const QString& origin, const QString& host);

public slots:
    void cleanupAndExit();
    void blockTabClose();
    void ipcMessageReceived();
    void showDictionaryWindow();
    void showDictionaryWindowEx(const QString& text);
    void windowDestroyed(CMainWindow* obj);
    void focusChanged(QWidget* old, QWidget* now);
    void updateProxy(bool useProxy);
    void updateProxyWithMenuUpdate(bool useProxy, bool forceMenuUpdate);
    void clearCaches();

    // Cookies sync
    void cookieAdded(const QNetworkCookie &cookie);
    void cookieRemoved(const QNetworkCookie &cookie);

    // ATLAS SSL
    void atlSSLCertErrors(const QSslCertificate& cert, const QStringList& errors,
                          const CIntList& errCodes);
};

extern CGlobalControl* gSet;

#endif // QGLOBALSETTINGS_H
