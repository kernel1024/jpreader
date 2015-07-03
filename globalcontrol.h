#ifndef QGLOBALSETTINGS_H
#define QGLOBALSETTINGS_H

#include <QObject>
#include <QWidget>
#include <QApplication>
#include <QDataStream>
#include <QString>
#include <QUrl>
#include <QUuid>
#include <QList>
#include <QAction>
#include <QFont>
#include <QThread>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QAuthenticator>
#include <QClipboard>
#include <QMutex>
#include <QTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDebug>

#include "specwidgets.h"
#include "logdisplay.h"

#define TE_NIFTY 0
#define TE_GOOGLE 1
#define TE_ATLAS 2
#define TE_BINGAPI 3
#define TECOUNT 4

#define SE_NONE 0
#define SE_RECOLL 2
#define SE_BALOO5 3

#define TM_ADDITIVE 0
#define TM_OVERWRITING 1
#define TM_TOOLTIP 2

#define LS_JAPANESE 0
#define LS_CHINESETRAD 1
#define LS_CHINESESIMP 2
#define LS_KOREAN 3
#define LSCOUNT 4

#define DBUS_NAME "org.jpreader.auxtranslator"
#define IPC_NAME "org.jpreader.ipc.main"

class CMainWindow;
class QSpecCookieJar;
class CLightTranslator;
class CAuxTranslator;
class CTranslator;
class CAuxDictionary;
class ArticleNetworkAccessManager;
class CGoldenDictMgr;
class QxtGlobalShortcut;
class CSettingsDlg;

class UrlHolder {
    friend QDataStream &operator<<(QDataStream &out, const UrlHolder &obj);
    friend QDataStream &operator>>(QDataStream &in, UrlHolder &obj);
public:
    QString title;
    QUrl url;
    QUuid uuid;
    UrlHolder();
    UrlHolder(QString title, QUrl url);
    UrlHolder &operator=(const UrlHolder& other);
    bool operator==(const UrlHolder &s) const;
    bool operator!=(const UrlHolder &s) const;
};

class DirStruct {
public:
    DirStruct();
    DirStruct(QString DirName, int Count);
    DirStruct &operator=(const DirStruct& other);
    QString dirName;
    int count;
};

class CGlobalControl : public QObject
{
    Q_OBJECT
public:
    explicit CGlobalControl(QApplication *parent);

    CMainWindow* activeWindow;
    QList<CMainWindow*> mainWindows;
    CLightTranslator* lightTranslator;
    CAuxTranslator* auxTranslatorDBus;
    QLocalServer* ipcServer;

    QAction* actionSelectionDictionary;
    QAction* actionGlobalTranslator;
    QIcon appIcon;
    CLogDisplay* logWindow;

    ArticleNetworkAccessManager * dictNetMan;
    CGoldenDictMgr * dictManager;
    QStringList dictPaths;
    QString dictIndexDir;
    CAuxDictionary* auxDictionary;

    QList<QColor> snippetColors;

    QMutex sortMutex;

    QMap<QString, QUrl> bookmarks;
    QList<UrlHolder> recycleBin;
    QList<UrlHolder> mainHistory;
    QStringList searchHistory;
    QStringList adblock;
    QFont overrideFont;
    bool overrideStdFonts;
    QString fontStandard, fontFixed, fontSerif, fontSansSerif;
    bool showTabCloseButtons;
    bool createCoredumps;

    bool overrideUserAgent;
    QString userAgent;
    QStringList userAgentHistory;

    int searchEngine;
    bool useAdblock;
    QString scpHost;
    QString scpParams;
    bool useScp;
    QString hostingDir;
    QString hostingUrl;
    QStringList createdFiles;
    int maxLimit;
    int translatorEngine;
    QString atlHost;
    int atlPort;
    bool emptyRestore;
    QString sysBrowser;
    QString sysEditor;
    int maxRecycled;
    QStringList atlHostHistory;
    QStringList scpHostHistory;
    QTimer settingsSaveTimer;
    QMutex settingsSaveMutex;
    bool debugNetReqLogging;
    QTimer tabsListTimer;
    bool restoreLoadChecked;

    QString savedAuxDir;
    QSpecNetworkAccessManager netAccess;
    QSpecCookieJar cookieJar;
    QString forcedCharset;

    QString lastClipboardContents;
    QString lastClipboardContentsUnformatted;
    bool lastClipboardIsHtml;

    bool globalContextTranslate;
    QxtGlobalShortcut* gctxTranHotkey;
    bool blockTabCloseActive;
    QColor forcedFontColor;

    int atlTcpRetryCount;
    int atlTcpTimeout;
    QString bingID;
    QString bingKey;

    QString proxyHost;
    QString proxyLogin;
    QString proxyPassword;
    int proxyPort;
    bool proxyUse;
    QNetworkProxy::ProxyType proxyType;
    QAction *actionUseProxy;

    bool ignoreSSLErrors;

    QAction *actionJSUsage;
    QAction *actionSnippetAutotranslate;

    QAction *actionForceNewTab;
    QAction *actionAutoTranslate;
    QAction *actionOverrideFont;
    QAction *actionAutoloadImages;
    QAction *actionOverrideFontColor;

    QAction *actionTMAdditive, *actionTMOverwriting, *actionTMTooltip;
    QAction *actionLSJapanese, *actionLSChineseTraditional;
    QAction *actionLSChineseSimplified, *actionLSKorean;
    QActionGroup *translationMode, *sourceLanguage;

    // Actions for Settings menu
    bool useOverrideFont();
    bool autoTranslate();
    bool forceAllLinksInNewTab();
    bool forceFontColor();

    // History lists append
    void appendRecycled(QString title, QUrl url);
    void appendSearchHistory(QStringList req);
    void appendMainHistory(UrlHolder& item);

    // Lists updaters
    void updateAllBookmarks();
    void updateAllRecycleBins();
    void updateAllCharsetLists();
    void updateAllQueryLists();
    void updateAllHistoryLists();

    // Ad-block
    bool isUrlBlocked(QUrl url);

    // Password management
    void readPassword(const QUrl &origin, QString &user, QString &password);
    void savePassword(const QUrl &origin, const QString &user, const QString &password);

    int getTranslationMode();
    int getSourceLanguage();
    QString getSourceLanguageID(int engineStd = TE_GOOGLE);
    QString getSourceLanguageIDStr(int engine, int engineStd = TE_GOOGLE);

    QString getSourceLanguageString(int srcLang);
    QString getTranslationEngineString(int engine);
    void setTranslationEngine(int engine);

protected:
    CSettingsDlg* dlg;
    bool cleaningState;
    void cleanTmpFiles();
    void startGlobalContextTranslate(const QString& text);

private:
    bool setupIPC();
    void sendIPCMessage(QLocalSocket *socket, const QString& msg);

signals:
    void startAuxTranslation();
    void stopTranslators();
    void settingsUpdated();

public slots:
    CMainWindow* addMainWindow(bool withSearch = false, bool withViewer = true);
    void settingsDlg();
    void windowDestroyed(CMainWindow* obj);
    void cleanupAndExit(bool appQuit = true);
    void focusChanged(QWidget* old, QWidget* now);
    void preShutdown();
    void closeLockTimer();
    void blockTabClose();
    void authentication(QNetworkReply *reply, QAuthenticator *authenticator);
    void clipboardChanged(QClipboard::Mode mode);
    void ipcMessageReceived();
    void globalContextTranslateReady(const QString& text);
    void clearCaches();
    void showDictionaryWindow(const QString& text = QString());

    // Settings management
    void writeSettings();
    void readSettings();
    void writeTabsList(bool clearList = false);
    void checkRestoreLoad(CMainWindow* w);
    void updateProxy(bool useProxy, bool forceMenuUpdate = false);
    void toggleJSUsage(bool useJS);
    void toggleAutoloadImages(bool loadImages);
};

extern CGlobalControl* gSet;

int compareStringLists(const QStringList& left, const QStringList& right);

#endif // QGLOBALSETTINGS_H
