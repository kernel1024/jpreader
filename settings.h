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

namespace CDefaults {
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
const int overrideTransFontSize = 12;
const int fontSizeMinimal = 12;
const int fontSizeDefault = 12;
const int fontSizeFixed = 12;
const int translatorCacheSize = 128;
const quint16 proxyPort = 3128;
const QSsl::SslProtocol atlProto = QSsl::SecureProtocols;
const QNetworkProxy::ProxyType proxyType = QNetworkProxy::NoProxy;
const CStructures::SearchEngine searchEngine = CStructures::seNone;
const CStructures::TranslationEngine translatorEngine = CStructures::teAtlas;
const bool jsLogConsole = true;
const bool dontUseNativeFileDialog = false;
const bool overrideStdFonts = false;
const bool overrideFontSizes = false;
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
const bool translatorCacheEnabled = false;
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

    QString atlHost;
    QString sysBrowser;
    QString sysEditor;

    QString savedAuxDir;
    QString savedAuxSaveDir;
    QString forcedCharset;

    QString atlToken;
    QString bingKey;
    QString yandexKey;
    QString awsRegion;
    QString awsAccessKey;
    QString awsSecretKey;

    QString proxyHost;
    QString proxyLogin;
    QString proxyPassword;

    QFont overrideTransFont;
    QColor forcedFontColor;

    QStringList userAgentHistory;
    QStringList atlHostHistory;
    QStringList charsetHistory;

    QVector<CLangPair> translatorPairs;
    CSslCertificateHash atlCerts;
    CSubsentencesMode subsentencesMode;

    int maxHistory { CDefaults::maxHistory };
    int maxRecent { CDefaults::maxRecent };
    int maxSearchLimit { CDefaults::maxSearchLimit };
    int maxRecycled { CDefaults::maxRecent };
    int atlTcpRetryCount { CDefaults::atlTcpRetryCount };
    int atlTcpTimeout { CDefaults::atlTcpTimeout };
    int maxAdblockWhiteList { CDefaults::maxAdblockWhiteList };
    int pdfImageMaxSize { CDefaults::pdfImageMaxSize };
    int pdfImageQuality { CDefaults::pdfImageQuality };
    int fontSizeMinimal { CDefaults::fontSizeMinimal };
    int fontSizeDefault { CDefaults::fontSizeDefault };
    int fontSizeFixed { CDefaults::fontSizeFixed };
    int translatorCacheSize { CDefaults::translatorCacheSize };
    quint16 atlPort { CDefaults::atlPort };
    quint16 proxyPort { CDefaults::proxyPort };
    QSsl::SslProtocol atlProto { CDefaults::atlProto };
    QNetworkProxy::ProxyType proxyType { CDefaults::proxyType };
    CStructures::SearchEngine searchEngine { CDefaults::searchEngine };
    CStructures::TranslationEngine translatorEngine { CDefaults::translatorEngine };

    bool jsLogConsole { CDefaults::jsLogConsole };
    bool dontUseNativeFileDialog { CDefaults::dontUseNativeFileDialog };
    bool overrideStdFonts { CDefaults::overrideStdFonts };
    bool overrideFontSizes { CDefaults::overrideFontSizes };
    bool showTabCloseButtons { CDefaults::showTabCloseButtons };
    bool createCoredumps { CDefaults::createCoredumps };
    bool overrideUserAgent { CDefaults::overrideUserAgent };
    bool useAdblock { CDefaults::useAdblock };
    bool useNoScript { CDefaults::useNoScript };
    bool emptyRestore { CDefaults::emptyRestore };
    bool debugNetReqLogging { CDefaults::debugNetReqLogging };
    bool debugDumpHtml { CDefaults::debugDumpHtml };
    bool globalContextTranslate { CDefaults::globalContextTranslate };
    bool proxyUse { CDefaults::proxyUse };
    bool proxyUseTranslator { CDefaults::proxyUseTranslator };
    bool ignoreSSLErrors { CDefaults::ignoreSSLErrors };
    bool pdfExtractImages { CDefaults::pdfExtractImages };
    bool pixivFetchImages { CDefaults::pixivFetchImages };
    bool translatorCacheEnabled { CDefaults::translatorCacheEnabled };

    explicit CSettings(QObject *parent = nullptr);

    void readSettings(QObject *control = nullptr);
    void setTranslationEngine(CStructures::TranslationEngine engine);

Q_SIGNALS:
    void adblockRulesUpdated();

private:
    QVector<QUrl> getTabsList() const;
    void writeTabsListPrivate(const QVector<QUrl> &tabList);
    bool readBinaryBigData(QObject* control, const QString &filename);
    void writeBinaryBigData(const QString &filename);

public Q_SLOTS:
    void writeSettings();
    void settingsDlg();

    void clearTabsList();
    void writeTabsList();

    void checkRestoreLoad(CMainWindow* w);

};

#endif // CSETTINGS_H
