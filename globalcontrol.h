#ifndef QGLOBALSETTINGS_H
#define QGLOBALSETTINGS_H

#include <QObject>
#include <QWidget>
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
#include <QDebug>

#include "mainwindow.h"
#include "settingsdlg.h"
#include "specwidgets.h"
#include "qtsingleapplication/src/qtsingleapplication.h"
#include "miniqxt/qxtglobalshortcut.h"
#include "logdisplay.h"

#define TE_NIFTY 0
#define TE_GOOGLE 1
#define TE_ATLAS 2
#define TE_BINGAPI 3

#define SE_NONE 0
#define SE_NEPOMUK 1
#define SE_RECOLL 2
#define SE_BALOO5 3

#define TM_ADDITIVE 1
#define TM_OVERWRITING 2
#define TM_TOOLTIP 3

#define LS_JAPANESE 1
#define LS_CHINESETRAD 2
#define LS_CHINESESIMP 3
#define LS_KOREAN 4

class CMainWindow;
class QSpecCookieJar;
class CLightTranslator;
class CAuxTranslator;
class CTranslator;

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
    explicit CGlobalControl(QtSingleApplication *parent);

    CMainWindow* activeWindow;
    QList<CMainWindow*> mainWindows;
    CLightTranslator* lightTranslator;
    CAuxTranslator* auxTranslatorDBus;

    QAction* actionSelectionDictionary;
    QAction* actionGlobalTranslator;
    QIcon appIcon;
    CLogDisplay* logWindow;

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
protected:
    CSettingsDlg* dlg;
    bool cleaningState;
    void cleanTmpFiles();
    void startGlobalContextTranslate(const QString& text);

signals:
    void startAuxTranslation();
    void stopTranslators();

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
    void ipcMessageReceived(const QString& msg);
    void globalContextTranslateReady(const QString& text);
    void clearCaches();

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

#endif // QGLOBALSETTINGS_H