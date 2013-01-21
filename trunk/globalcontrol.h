#ifndef QGLOBALSETTINGS_H
#define QGLOBALSETTINGS_H

#include <QtCore>
#include <QtGui>
#include <QtNetwork>
#include <QtDBus>
#include "mainwindow.h"
#include "settingsdlg.h"
#include "specwidgets.h"
#include "qtsingleapplication.h"
#include <QxtGlobalShortcut>

#define TE_NIFTY 0
#define TE_GOOGLE 1
#define TE_ATLAS 2

class CMainWindow;
class QSpecCookieJar;
class CLightTranslator;
class CAuxTranslator;

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

    QAction* actionGlobalTranslator;
    QSystemTrayIcon trayIcon;
    QIcon appIcon;

    QList<QColor> snippetColors;

    QMutex sortMutex;

    QMap<QString, QUrl> bookmarks;
    QList<UrlHolder> recycleBin;
    QList<UrlHolder> mainHistory;
    QStringList searchHistory;
    QStringList adblock;
    bool useOverrideFont;
    QFont overrideFont;
    bool overrideStdFonts;
    QString fontStandard, fontFixed, fontSerif, fontSansSerif;

    bool useAdblock;
    int maxTranThreads;
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
    QString sysBrowser;
    QString sysEditor;
    int maxRecycled;

    QString savedAuxDir;
    QSpecNetworkAccessManager netAccess;
    QSpecCookieJar cookieJar;
    QList<QThread*> tranThreads;
    QString forcedCharset;

    QString lastClipboardContents;
    QString lastClipboardContentsUnformatted;
    bool lastClipboardIsHtml;

    bool globalContextTranslate;
    QxtGlobalShortcut* gctxTranHotkey;
    bool autoTranslate;
    bool blockTabCloseActive;

    bool forceFontColor;
    QColor forcedFontColor;

    // History lists append
    void appendRecycled(QString title, QUrl url);
    void appendSearchHistory(QStringList req);
    void appendTranThread(QThread* tran);
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
protected:
    CSettingsDlg* dlg;
    bool cleaningState;
    void cleanTmpFiles();
    void startGlobalContextTranslate(const QString& text);

signals:
    void startAuxTranslation();

public slots:
    CMainWindow* addMainWindow(bool withSearch = false, bool withViewer = true);
    void settingsDlg();
    void windowDestroyed(CMainWindow* obj);
    void cleanupAndExit(bool appQuit = true);
    void trayClicked(QSystemTrayIcon::ActivationReason reason);
    void updateTrayIconState(bool state = false);
    void focusChanged(QWidget* old, QWidget* now);
    void preShutdown();
    void tranFinished();
    void closeLockTimer();
    void blockTabClose();
    void authentication(QNetworkReply *reply, QAuthenticator *authenticator);
    void clipboardChanged(QClipboard::Mode mode);
    void ipcMessageReceived(const QString& msg);
    void globalContextTranslateReady(const QString& text);

    // Settings management
    void writeSettings();
    void readSettings();
};

extern CGlobalControl* gSet;

#endif // QGLOBALSETTINGS_H
