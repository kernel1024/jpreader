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

#include "structures.h"

class CMainWindow;
class CSettingsPrivate;

namespace CSettingsDefault {
const int maxHistory = 5000;
const int maxRecent = 10;
const int maxSearchLimit = 1000;
const int maxRecycled = 20;
const int atlPort = 18000;
const int atlTcpRetryCount = 3;
const int atlTcpTimeout = 2;
const int maxAdblockWhiteList = 5000;
const int pdfImageMaxSize = 250;
const int pdfImageQuality = 75;
const int overrideFontSize = 12;
const quint16 proxyPort = 3128;
const QSsl::SslProtocol atlProto = QSsl::SecureProtocols;
const QNetworkProxy::ProxyType proxyType = QNetworkProxy::NoProxy;
const SearchEngine searchEngine = seNone;
const TranslationEngine translatorEngine = teAtlas;
const bool jsLogConsole = true;
const bool dontUseNativeFileDialog = false;
const bool overrideStdFonts = false;
const bool showTabCloseButtons = true;
const bool createCoredumps = false;
const bool overrideUserAgent = false;
const bool useAdblock = false;
const bool useNoScript = false;
const bool useScp = false;
const bool emptyRestore = false;
const bool debugNetReqLogging = false;
const bool debugDumpHtml = false;
const bool globalContextTranslate = false;
const bool proxyUse = false;
const bool proxyUseTranslator = false;
const bool ignoreSSLErrors = false;
const bool pdfExtractImages = true;
const bool pixivFetchImages = false;
const auto fontFixed = "Courier New";
const auto fontSerif = "Times New Roman";
const auto fontSansSerif = "Verdana";
const auto sysBrowser = "konqueror";
const auto sysEditor = "kwrite";
}


class CSettings : public QObject
{
    Q_OBJECT
public:
    static const QVector<QColor> snippetColors;
    static const QUrl::FormattingOptions adblockUrlFmt;

    QStringList dictPaths;
    QString dictIndexDir;
    QString defaultSearchEngine;
    QString fontStandard;
    QString fontFixed;
    QString fontSerif;
    QString fontSansSerif;
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
    CSslCertificateHash atlCerts;

    int maxHistory { CSettingsDefault::maxHistory };
    int maxRecent { CSettingsDefault::maxRecent };
    int maxSearchLimit { CSettingsDefault::maxSearchLimit };
    int maxRecycled { CSettingsDefault::maxRecent };
    int atlTcpRetryCount { CSettingsDefault::atlTcpRetryCount };
    int atlTcpTimeout { CSettingsDefault::atlTcpTimeout };
    int maxAdblockWhiteList { CSettingsDefault::maxAdblockWhiteList };
    int pdfImageMaxSize { CSettingsDefault::pdfImageMaxSize };
    int pdfImageQuality { CSettingsDefault::pdfImageQuality };
    quint16 atlPort { CSettingsDefault::atlPort };
    quint16 proxyPort { CSettingsDefault::proxyPort };
    QSsl::SslProtocol atlProto { CSettingsDefault::atlProto };
    QNetworkProxy::ProxyType proxyType { CSettingsDefault::proxyType };
    SearchEngine searchEngine { CSettingsDefault::searchEngine };
    TranslationEngine translatorEngine { CSettingsDefault::translatorEngine };

    bool jsLogConsole { CSettingsDefault::jsLogConsole };
    bool dontUseNativeFileDialog { CSettingsDefault::dontUseNativeFileDialog };
    bool overrideStdFonts { CSettingsDefault::overrideStdFonts };
    bool showTabCloseButtons { CSettingsDefault::showTabCloseButtons };
    bool createCoredumps { CSettingsDefault::createCoredumps };
    bool overrideUserAgent { CSettingsDefault::overrideUserAgent };
    bool useAdblock { CSettingsDefault::useAdblock };
    bool useNoScript { CSettingsDefault::useNoScript };
    bool useScp { CSettingsDefault::useScp };
    bool emptyRestore { CSettingsDefault::emptyRestore };
    bool debugNetReqLogging { CSettingsDefault::debugNetReqLogging };
    bool debugDumpHtml { CSettingsDefault::debugDumpHtml };
    bool globalContextTranslate { CSettingsDefault::globalContextTranslate };
    bool proxyUse { CSettingsDefault::proxyUse };
    bool proxyUseTranslator { CSettingsDefault::proxyUseTranslator };
    bool ignoreSSLErrors { CSettingsDefault::ignoreSSLErrors };
    bool pdfExtractImages { CSettingsDefault::pdfExtractImages };
    bool pixivFetchImages { CSettingsDefault::pixivFetchImages };

    explicit CSettings(QObject *parent = nullptr);

    void readSettings(QObject *control = nullptr);
    void setTranslationEngine(TranslationEngine engine);

private:
    QVector<QUrl> getTabsList() const;
    void writeTabsListPrivate(const QVector<QUrl> &tabList);

public Q_SLOTS:
    void writeSettings();
    void settingsDlg();

    void clearTabsList();
    void writeTabsList();

    void checkRestoreLoad(CMainWindow* w);

};

#endif // CSETTINGS_H
