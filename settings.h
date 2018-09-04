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

    QStringList dictPaths;
    QString dictIndexDir;
    QString defaultSearchEngine;
    QString fontStandard, fontFixed, fontSerif, fontSansSerif;
    QString userAgent;

    QString scpHost;
    QString scpParams;
    QString hostingDir;
    QString hostingUrl;
    QString atlHost;
    QString sysBrowser;
    QString sysEditor;

    QString savedAuxDir;
    QString savedAuxSaveDir;
    QString forcedCharset;

    QSsl::SslProtocol atlProto;
    QString atlToken;
    QString bingKey;
    QString yandexKey;

    QString proxyHost;
    QString proxyLogin;
    QString proxyPassword;
    QNetworkProxy::ProxyType proxyType;

    QFont overrideFont;
    QColor forcedFontColor;

    QStringList userAgentHistory;
    QStringList atlHostHistory;
    QStringList scpHostHistory;
    QStringList charsetHistory;

    QList<CLangPair> translatorPairs;

    int maxHistory;
    int maxRecent;
    int searchEngine;
    int maxSearchLimit;
    int translatorEngine;
    int maxRecycled;
    int atlTcpRetryCount;
    int atlTcpTimeout;
    int maxAdblockWhiteList;
    quint16 atlPort;
    quint16 proxyPort;

    bool jsLogConsole;
    bool dontUseNativeFileDialog;
    bool overrideStdFonts;
    bool showTabCloseButtons;
    bool createCoredumps;
    bool overrideUserAgent;
    bool useAdblock;
    bool useScp;
    bool emptyRestore;
    bool debugNetReqLogging;
    bool debugDumpHtml;
    bool globalContextTranslate;
    bool proxyUse;
    bool proxyUseTranslator;
    bool ignoreSSLErrors;

    explicit CSettings(QObject *parent = nullptr);

    void readSettings(QObject *control = nullptr);
    void setTranslationEngine(int engine);

private:
    QTimer settingsSaveTimer;
    QMutex settingsSaveMutex;
    CSettingsDlg* dlg;
    int settingsDlgWidth, settingsDlgHeight;
    bool restoreLoadChecked;
    QList<QUrl> getTabsList() const;
    void writeTabsListPrivate(const QList<QUrl> tabList);

signals:
    void settingsUpdated();

public slots:
    void writeSettings();
    void settingsDlg();

    void clearTabsList();
    void writeTabsList();

    void checkRestoreLoad(CMainWindow* w);

};

#endif // CSETTINGS_H
