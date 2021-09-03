#ifndef CSETTINGS_H
#define CSETTINGS_H

#include <QObject>
#include <QString>
#include <QNetworkProxy>
#include <QKeySequence>
#include <QSsl>
#include <QFont>
#include <QColor>
#include <QMutex>
#include <QTimer>
#include <QUrl>
#include <QDir>

#include "structures.h"

class CMainWindow;
class CSettingsPrivate;

namespace CDefaults {
const int maxHistory = 5000;
const int maxRecent = 10;
const int maxSearchLimit = 1000;
const int maxRecycled = 20;
const int atlPort = 18000;
const int translatorRetryCount = 10;
const int maxAdblockWhiteList = 5000;
const int pdfImageMaxSize = 250;
const int pdfImageQuality = 75;
const int overrideTransFontSize = 12;
const int fontSizeMinimal = 12;
const int fontSizeDefault = 12;
const int fontSizeFixed = 12;
const int translatorCacheSize = 128;
const int xapianStartDelay = 30;
const quint16 proxyPort = 3128;
const QSsl::SslProtocol atlProto = QSsl::SecureProtocols;
const QNetworkProxy::ProxyType proxyType = QNetworkProxy::NoProxy;
const CStructures::SearchEngine searchEngine = CStructures::seNone;
const CStructures::TranslationEngine translatorEngine = CStructures::teAtlas;
const CStructures::AliCloudTranslatorMode aliCloudTranslatorMode = CStructures::aliTranslatorGeneral;
const CStructures::PixivFetchCoversMode pixivFetchCovers = CStructures::pxfmLazyFetch;
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
const bool downloaderCleanCompleted = false;
const auto fontFixed = "Courier New";
const auto fontSerif = "Times New Roman";
const auto fontSansSerif = "Verdana";
const auto sysBrowser = "konqueror";
const auto sysEditor = "kwrite";
}

class CGlobalControl;

class CSettings : public QObject
{
    Q_OBJECT
public:
    static const QVector<QColor> graphColors;
    static const QVector<QColor> snippetColors;
    static const QUrl::FormattingOptions adblockUrlFmt;

    QStringList dictPaths;
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
    QString yandexCloudApiKey;
    QString yandexCloudFolderID;
    QString gcpJsonKeyFile;
    QString aliAccessKeyID;
    QString aliAccessKeySecret;

    QString xapianStemmerLang;

    QString proxyHost;
    QString proxyLogin;
    QString proxyPassword;

    QFont overrideTransFont;
    QColor forcedFontColor;

    QStringList userAgentHistory;
    QStringList atlHostHistory;
    QStringList charsetHistory;
    QStringList xapianIndexDirList;

    QKeySequence gctxSequence;
    QKeySequence autofillSequence;

    QVector<CLangPair> translatorPairs;
    CSslCertificateHash sslTrustedInvalidCerts;
    CTranslatorStatistics translatorStatistics;
    CSelectedLangPairs selectedLangPairs;

    int maxHistory { CDefaults::maxHistory };
    int maxRecent { CDefaults::maxRecent };
    int maxSearchLimit { CDefaults::maxSearchLimit };
    int maxRecycled { CDefaults::maxRecent };
    int maxAdblockWhiteList { CDefaults::maxAdblockWhiteList };
    int pdfImageMaxSize { CDefaults::pdfImageMaxSize };
    int pdfImageQuality { CDefaults::pdfImageQuality };
    int fontSizeMinimal { CDefaults::fontSizeMinimal };
    int fontSizeDefault { CDefaults::fontSizeDefault };
    int fontSizeFixed { CDefaults::fontSizeFixed };
    int translatorCacheSize { CDefaults::translatorCacheSize };
    int translatorRetryCount { CDefaults::translatorRetryCount };
    int xapianStartDelay { CDefaults::xapianStartDelay };
    quint16 atlPort { CDefaults::atlPort };
    quint16 proxyPort { CDefaults::proxyPort };
    QSsl::SslProtocol atlProto { CDefaults::atlProto };
    QNetworkProxy::ProxyType proxyType { CDefaults::proxyType };
    CStructures::SearchEngine searchEngine { CDefaults::searchEngine };
    CStructures::TranslationEngine translatorEngine { CDefaults::translatorEngine };
    CStructures::TranslationEngine previousTranslatorEngine { CDefaults::translatorEngine };
    CStructures::AliCloudTranslatorMode aliCloudTranslatorMode { CDefaults::aliCloudTranslatorMode };
    CStructures::PixivFetchCoversMode pixivFetchCovers { CDefaults::pixivFetchCovers };

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
    bool downloaderCleanCompleted { CDefaults::downloaderCleanCompleted };

    explicit CSettings(CGlobalControl *parent);

    void readSettings(QObject *control = nullptr);
    void setTranslationEngine(CStructures::TranslationEngine engine);
    void updateXapianIndexDirs(const QStringList& directories);
    QString getSelectedLangPair(CStructures::TranslationEngine engine) const;
    CStructures::TranslationEngine getTranslationEngineFromName(const QString& name, bool *ok) const;

Q_SIGNALS:
    void adblockRulesUpdated();

private:
    QVector<QUrl> getTabsList() const;
    void writeTabsListPrivate(const QVector<QUrl> &tabList);
    bool readBinaryBigData(QObject* control, const QString &dirname);
    void writeBinaryBigData(const QString &dirname);
    QByteArray readByteArray(const QDir &directory, const QString &name);
    bool writeByteArray(const QDir &directory, const QString &name, const QByteArray &data);
    bool writeData(const QDir &directory, const QString& name, const QVariant& data);
    QVariant readData(const QDir &directory, const QString &name,
                      const QVariant &defaultValue = QVariant());
public Q_SLOTS:
    void writeSettings();
    void settingsTab();

    void clearTabsList();
    void writeTabsList();

    void checkRestoreLoad(CMainWindow* w);

};

#endif // CSETTINGS_H
