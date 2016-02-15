#ifndef QGLOBALSETTINGS_H
#define QGLOBALSETTINGS_H

#include <QObject>
#include <QWidget>
#include <QApplication>
#include <QString>
#include <QUrl>
#include <QAction>
#include <QThread>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QAuthenticator>
#include <QClipboard>
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

class CMainWindow;
class CLightTranslator;
class CAuxTranslator;
class CTranslator;
class CAuxDictionary;
class ArticleNetworkAccessManager;
class CGoldenDictMgr;
class QxtGlobalShortcut;
class CDownloadManager;

class CGlobalControl : public QObject
{
    Q_OBJECT
public:
    explicit CGlobalControl(QApplication *parent);

    CSettings settings;

    CMainWindow* activeWindow;
    QList<CMainWindow*> mainWindows;
    CLightTranslator* lightTranslator;
    CAuxTranslator* auxTranslatorDBus;
    QLocalServer* ipcServer;

    QIcon appIcon;
    CLogDisplay* logWindow;
    CDownloadManager* downloadManager;

    ArticleNetworkAccessManager * dictNetMan;
    CGoldenDictMgr * dictManager;
    CAuxDictionary* auxDictionary;

    QWebEngineProfile *webProfile;
    QNetworkAccessManager *auxNetManager;

    QStringList recentFiles;

    QStrHash ctxSearchEngines;

    QBookmarksMap bookmarks;
    QUHList recycleBin;
    QUHList mainHistory;
    QStringList searchHistory;

    QHash<QString,QIcon> favicons;

    CAdBlockList adblock;
    QMutex adblockMutex;
    QStringList createdFiles;

    QTimer tabsListTimer;

    QxtGlobalShortcut* gctxTranHotkey;
    bool blockTabCloseActive;

    QSslCertificateHash atlCerts;

    QAction *actionUseProxy;

    QAction* actionSelectionDictionary;
    QAction* actionGlobalTranslator;

    QAction *actionJSUsage;
    QAction *actionSnippetAutotranslate;
    QAction *actionLogNetRequests;

    QAction *actionAutoTranslate;
    QAction *actionOverrideFont;
    QAction *actionAutoloadImages;
    QAction *actionOverrideFontColor;
    QAction *actionTranslateSubSentences;

    QAction *actionTMAdditive, *actionTMOverwriting, *actionTMTooltip;
    QAction *actionLSJapanese, *actionLSChineseTraditional;
    QAction *actionLSChineseSimplified, *actionLSKorean;
    QActionGroup *translationMode, *sourceLanguage;

    // Actions for Settings menu
    bool useOverrideFont();
    bool autoTranslate();
    bool forceFontColor();

    // History lists append
    void appendRecycled(QString title, QUrl url);
    void appendSearchHistory(QStringList req);
    void appendMainHistory(UrlHolder& item);
    bool updateMainHistoryTitle(UrlHolder& item, QString newTitle);
    void appendRecent(QString filename);

    // Lists updaters
    void updateAllBookmarks();
    void updateAllRecycleBins();
    void updateAllCharsetLists();
    void updateAllQueryLists();
    void updateAllHistoryLists();
    void updateAllRecentLists();

    // Ad-block
    bool isUrlBlocked(QUrl url);
    bool isUrlBlocked(QUrl url, QString& filter);
    void adblockAppend(QString url);
    void adblockAppend(CAdBlockRule url);
    void adblockAppend(QList<CAdBlockRule> urls);

    // Password management
    void readPassword(const QUrl &origin, QString &user, QString &password);
    void savePassword(const QUrl &origin, const QString &user, const QString &password);

    int getTranslationMode();
    int getSourceLanguage();
    QString getSourceLanguageID(int engineStd = TE_GOOGLE);
    QString getSourceLanguageIDStr(int engine, int engineStd = TE_GOOGLE);

    QString getSourceLanguageString(int srcLang);
    QString getTranslationEngineString(int engine);

    void showLightTranslator(const QString& text = QString());

    QUrl createSearchUrl(const QString& text, const QString& engine = QString());

private:
    bool restoreLoadChecked;
    bool cleaningState;
    bool atlCertErrorInteractive;

    bool setupIPC();
    void sendIPCMessage(QLocalSocket *socket, const QString& msg);
    void cleanTmpFiles();
    void startGlobalContextTranslate(const QString& text);

signals:
    void startAuxTranslation();
    void stopTranslators();

public slots:
    CMainWindow* addMainWindow(bool withSearch = false, bool withViewer = true);
    void windowDestroyed(CMainWindow* obj);
    void cleanupAndExit();
    void focusChanged(QWidget* old, QWidget* now);
    void closeLockTimer();
    void blockTabClose();
    void clipboardChanged(QClipboard::Mode mode);
    void ipcMessageReceived();
    void globalContextTranslateReady(const QString& text);
    void showDictionaryWindow(const QString& text = QString());

    // Settings management
    void writeTabsList(bool clearList = false);
    void checkRestoreLoad(CMainWindow* w);
    void updateProxy(bool useProxy, bool forceMenuUpdate = false);
    void toggleJSUsage(bool useJS);
    void toggleAutoloadImages(bool loadImages);
    void toggleLogNetRequests(bool logRequests);

    // Cookies sync
    void cookieAdded(const QNetworkCookie &cookie);
    void cookieRemoved(const QNetworkCookie &cookie);

    // ATLAS SSL
    void atlSSLCertErrors(const QSslCertificate& cert, const QStringList& errors, const QIntList& errCodes);
};

extern CGlobalControl* gSet;

#endif // QGLOBALSETTINGS_H
