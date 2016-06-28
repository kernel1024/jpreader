#ifndef CSETTINGS_H
#define CSETTINGS_H

#include <QObject>
#include <QString>
#include <QNetworkProxy>
#include <QSsl>
#include <QFont>
#include <QColor>
#include <QMutex>
#include <QTimer>

#include "settingsdlg.h"
#include "mainwindow.h"

class CSettingsDlg;
class CMainWindow;

class CSettings : public QObject
{
    Q_OBJECT
public:
    QList<QColor> snippetColors;

    bool jsLogConsole;
    QStringList dictPaths;
    QString dictIndexDir;
    QString defaultSearchEngine;
    int maxHistory;
    int maxRecent;
    bool overrideStdFonts;
    QString fontStandard, fontFixed, fontSerif, fontSansSerif;
    bool showTabCloseButtons;
    bool createCoredumps;
    bool showFavicons;

    bool overrideUserAgent;
    QString userAgent;

    bool useAdblock;

    int searchEngine;
    QString scpHost;
    QString scpParams;
    bool useScp;
    QString hostingDir;
    QString hostingUrl;
    int maxSearchLimit;
    int translatorEngine;
    QString atlHost;
    int atlPort;
    bool emptyRestore;
    QString sysBrowser;
    QString sysEditor;
    int maxRecycled;

    bool debugNetReqLogging;
    bool debugDumpHtml;

    QString savedAuxDir;
    QString savedAuxSaveDir;
    QString forcedCharset;
    bool globalContextTranslate;

    int atlTcpRetryCount;
    int atlTcpTimeout;
    QSsl::SslProtocol atlProto;
    QString atlToken;
    QString bingID;
    QString bingKey;
    QString yandexKey;

    QString proxyHost;
    QString proxyLogin;
    QString proxyPassword;
    int proxyPort;
    bool proxyUse;
    QNetworkProxy::ProxyType proxyType;
    bool proxyUseTranslator;

    bool ignoreSSLErrors;

    QFont overrideFont;
    QColor forcedFontColor;

    QStringList userAgentHistory;
    QStringList atlHostHistory;
    QStringList scpHostHistory;
    QStringList charsetHistory;

    explicit CSettings(QObject *parent = 0);

    void readSettings(QObject *control = 0);
    void setTranslationEngine(int engine);

private:
    QTimer settingsSaveTimer;
    QMutex settingsSaveMutex;
    CSettingsDlg* dlg;
    bool restoreLoadChecked;
    int settingsDlgWidth, settingsDlgHeight;

signals:
    void settingsUpdated();

public slots:
    void writeSettings();
    void settingsDlg();

    void writeTabsList(bool clearList = false);
    void checkRestoreLoad(CMainWindow* w);

};

#endif // CSETTINGS_H
