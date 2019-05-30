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
#include <QUrl>

#include "settingsdlg.h"

class CSettingsDlg;
class CMainWindow;

class CSettings : public QObject
{
    Q_OBJECT
public:
    static const QVector<QColor> snippetColors;
    static const QUrl::FormattingOptions adblockUrlFmt;

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

    QString atlToken;
    QString bingKey;
    QString yandexKey;

    QString proxyHost;
    QString proxyLogin;
    QString proxyPassword;

    QFont overrideFont;
    QColor forcedFontColor;

    QStringList userAgentHistory;
    QStringList atlHostHistory;
    QStringList scpHostHistory;
    QStringList charsetHistory;

    QVector<CLangPair> translatorPairs;

    int maxHistory;
    int maxRecent;
    SearchEngine searchEngine;
    int maxSearchLimit;
    TranslationEngine translatorEngine;
    int maxRecycled;
    int atlTcpRetryCount;
    int atlTcpTimeout;
    int maxAdblockWhiteList;
    int pdfImageMaxSize;
    int pdfImageQuality;
    bool pixivFetchImages;
    quint16 atlPort;
    quint16 proxyPort;
    QSsl::SslProtocol atlProto;
    QNetworkProxy::ProxyType proxyType;

    bool jsLogConsole;
    bool dontUseNativeFileDialog;
    bool overrideStdFonts;
    bool showTabCloseButtons;
    bool createCoredumps;
    bool overrideUserAgent;
    bool useAdblock;
    bool useNoScript;
    bool useScp;
    bool emptyRestore;
    bool debugNetReqLogging;
    bool debugDumpHtml;
    bool globalContextTranslate;
    bool proxyUse;
    bool proxyUseTranslator;
    bool ignoreSSLErrors;
    bool pdfExtractImages;

    explicit CSettings(QObject *parent = nullptr);

    void readSettings(QObject *control = nullptr);
    void setTranslationEngine(TranslationEngine engine);

private:
    QTimer settingsSaveTimer;
    QMutex settingsSaveMutex;
    CSettingsDlg* dlg;
    int settingsDlgWidth, settingsDlgHeight;
    bool restoreLoadChecked;
    QVector<QUrl> getTabsList() const;
    void writeTabsListPrivate(const QVector<QUrl> &tabList);

Q_SIGNALS:
    void settingsUpdated();

public Q_SLOTS:
    void writeSettings();
    void settingsDlg();

    void clearTabsList();
    void writeTabsList();

    void checkRestoreLoad(CMainWindow* w);

};

#endif // CSETTINGS_H
