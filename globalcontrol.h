#ifndef QGLOBALSETTINGS_H
#define QGLOBALSETTINGS_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QUrl>
#include <QLocalSocket>
#include <QSslCertificate>
#include "adblockrule.h"
#include "structures.h"
#include "settings.h"
#include "globalui.h"
#include "userscript.h"

class CLogDisplay;
class CDownloadManager;
class CGlobalControlPrivate;
class BookmarksManager;
class ArticleNetworkAccessManager;
class CGoldenDictMgr;

class CGlobalControl : public QObject
{
    friend class CSettings;
    friend class CGlobalUI;
    friend class CSettingsDlg;

    Q_OBJECT
public:
    explicit CGlobalControl(QApplication *parent, int aInspectorPort);
    ~CGlobalControl() override;

    // History lists
    const CUrlHolderVector &recycleBin() const;
    const CUrlHolderVector &mainHistory() const;
    const QStringList &recentFiles() const;
    const QStringList &searchHistory() const;
    void appendRecycled(const QString &title, const QUrl &url);
    void removeRecycledItem(int idx);
    void appendSearchHistory(const QStringList &req);
    void appendMainHistory(const CUrlHolder &item);
    bool updateMainHistoryTitle(const CUrlHolder &item, const QString &newTitle);
    void appendRecent(const QString &filename);
    void appendCreatedFiles(const QString &filename);

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
    QString getLanguageName(const QString &bcp47Name) const;
    void showLightTranslator(const QString& text = QString());

    // Misc
    QUrl createSearchUrl(const QString& text, const QString& engine = QString()) const;
    QStringList getSearchEngines() const;
    QString makeTmpFileName(const QString &suffix, bool withDir = false);
    bool isIPCStarted() const;
    const CSettings *settings() const;
    const CGlobalUI *ui() const;
    void writeSettings();
    void addFavicon(const QString& key, const QIcon &icon);
    void setTranslationEngine(TranslationEngine engine);

    // Chromium
    QUrl getInspectorUrl() const;
    QWebEngineProfile* webProfile() const;

    // UI
    QIcon appIcon() const;
    void setSavedAuxSaveDir(const QString& dir);
    void setSavedAuxDir(const QString& dir);
    CMainWindow* addMainWindow();
    CMainWindow* addMainWindowEx(bool withSearch, bool withViewer,
                                 const QVector<QUrl> &viewerUrls = { });
    void settingsDialog();
    QRect getLastMainWindowGeometry() const;
    QList<QAction*> getTranslationLanguagesActions() const;
    bool isBlockTabCloseActive() const;

    // Owned widgets
    CMainWindow *activeWindow() const;
    BookmarksManager *bookmarksManager() const;
    CDownloadManager* downloadManager() const;
    CLogDisplay* logWindow() const;
    ArticleNetworkAccessManager *dictionaryNetworkAccessManager() const;
    QNetworkAccessManager* auxNetworkAccessManager() const;
    const QHash<QString,QIcon> &favicons() const;
    CGoldenDictMgr* dictionaryManager() const;

private:
    Q_DISABLE_COPY(CGlobalControl)
    Q_DECLARE_PRIVATE_D(dptr,CGlobalControl)

    QScopedPointer<CGlobalControlPrivate> dptr;
    QScopedPointer<CSettings> m_settings;
    QScopedPointer<CGlobalUI> m_ui;

    bool setupIPC();
    void sendIPCMessage(QLocalSocket *socket, const QString& msg);
    void cleanTmpFiles();
    void initLanguagesList();

Q_SIGNALS:
    void startAuxTranslation();
    void stopTranslators();

    void settingsUpdated();
    void updateAllBookmarks();
    void updateAllRecycleBins();
    void updateAllCharsetLists();
    void updateAllQueryLists();
    void updateAllHistoryLists();
    void updateAllRecentLists();
    void updateAllLanguagesLists();

    void addAdBlockWhiteListUrl(const QString& url);
    void addNoScriptPageHost(const QString& origin, const QString& host);

public Q_SLOTS:
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
    void forceCharset();

    // Cookies sync
    void cookieAdded(const QNetworkCookie &cookie);
    void cookieRemoved(const QNetworkCookie &cookie);

    // ATLAS SSL
    void atlSSLCertErrors(const QSslCertificate& cert, const QStringList& errors,
                          const CIntList& errCodes);
};

extern CGlobalControl* gSet;

#endif // QGLOBALSETTINGS_H
