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
#include "manga/scalefilter.h"

class CMainWindow;
class CSettingsPrivate;

namespace CDefaults {
const int oneK = 1000;
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
const int domWorkerReplyTimeoutSec = 60;
const int mangaMagnifySize = 150;
const int mangaScrollDelta = 120;
const int mangaScrollFactor = 5;
const int mangaCacheWidth = 6;
const int downloadsLimit = 0;
const int tokensMaxCountCombined = 1024;
const unsigned int mangaBackgroundColor = 0x303030;
const double mangaResizeBlur = 1.0;
const double openaiTemperature = 1.0;
const double openaiTopP = 1.0;
const double openaiPresencePenalty = 0.0;
const double openaiFrequencyPenalty = 0.0;
const quint16 proxyPort = 3128;
const QSsl::SslProtocol atlProto = QSsl::SecureProtocols;
const QNetworkProxy::ProxyType proxyType = QNetworkProxy::NoProxy;
const CStructures::SearchEngine searchEngine = CStructures::seNone;
const CStructures::TranslationEngine translatorEngine = CStructures::teAtlas;
const CStructures::AliCloudTranslatorMode aliCloudTranslatorMode = CStructures::aliTranslatorGeneral;
const CStructures::PixivFetchCoversMode pixivFetchCovers = CStructures::pxfmLazyFetch;
const CStructures::PixivMangaPageSize pixivMangaPageSize = CStructures::pxmpOriginal;
const CStructures::DeeplAPIMode deeplAPIMode = CStructures::deeplAPIModeFree;
const CStructures::DeeplAPISplitSentences deeplAPISplitSentences = CStructures::deeplAPISplitDefault;
const CStructures::DeeplAPIFormality deeplAPIFormality = CStructures::deeplAPIFormalityDefault;
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
const bool mangaUseFineRendering = true;
const auto fontFixed = "Courier New";
const auto fontSerif = "Times New Roman";
const auto fontSansSerif = "Verdana";
const auto sysBrowser = "konqueror";
const auto sysEditor = "kwrite";
const auto propXapianInotifyTimer = "inotify";
const auto promtNmtServer = "https://pts.promt.ru/pts";
}

class CGlobalControl;

class CSettings : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CSettings)
public:
    static const QVector<QColor> graphColors;
    static const QVector<QColor> snippetColors;
    static const QUrl::FormattingOptions adblockUrlFmt;

    QStringList openAIModels;
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
    QString promtNmtAPIKey;
    QString promtNmtServer;
    QString deeplAPIKey;
    QString openaiAPIKey;
    QString openaiTranslationModel;

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

    QColor mangaBackgroundColor { QColor(CDefaults::mangaBackgroundColor) };

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
    int domWorkerReplyTimeoutSec { CDefaults::domWorkerReplyTimeoutSec };
    int mangaMagnifySize { CDefaults::mangaMagnifySize };
    int mangaScrollDelta { CDefaults::mangaScrollDelta };
    int mangaScrollFactor { CDefaults::mangaScrollFactor };
    int mangaCacheWidth { CDefaults::mangaCacheWidth };
    int downloadsLimit { CDefaults::downloadsLimit };
    int tokensMaxCountCombined { CDefaults::tokensMaxCountCombined };
    quint16 atlPort { CDefaults::atlPort };
    quint16 proxyPort { CDefaults::proxyPort };
    QSsl::SslProtocol atlProto { CDefaults::atlProto };
    QNetworkProxy::ProxyType proxyType { CDefaults::proxyType };
    CStructures::SearchEngine searchEngine { CDefaults::searchEngine };
    CStructures::TranslationEngine translatorEngine { CDefaults::translatorEngine };
    CStructures::TranslationEngine previousTranslatorEngine { CDefaults::translatorEngine };
    CStructures::AliCloudTranslatorMode aliCloudTranslatorMode { CDefaults::aliCloudTranslatorMode };
    CStructures::PixivFetchCoversMode pixivFetchCovers { CDefaults::pixivFetchCovers };
    CStructures::PixivMangaPageSize pixivMangaPageSize { CDefaults::pixivMangaPageSize };
    Blitz::ScaleFilterType mangaUpscaleFilter { Blitz::ScaleFilterType::UndefinedFilter };
    Blitz::ScaleFilterType mangaDownscaleFilter { Blitz::ScaleFilterType::UndefinedFilter };
    CStructures::DeeplAPIMode deeplAPIMode { CDefaults::deeplAPIMode };
    CStructures::DeeplAPISplitSentences deeplAPISplitSentences { CDefaults::deeplAPISplitSentences };
    CStructures::DeeplAPIFormality deeplAPIFormality { CDefaults::deeplAPIFormality };
    double mangaResizeBlur { CDefaults::mangaResizeBlur };
    double openaiTemperature { CDefaults::openaiTemperature };
    double openaiTopP { CDefaults::openaiTopP };
    double openaiPresencePenalty { CDefaults::openaiPresencePenalty };
    double openaiFrequencyPenalty { CDefaults::openaiFrequencyPenalty };

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
    bool mangaUseFineRendering { CDefaults::mangaUseFineRendering };

    explicit CSettings(CGlobalControl *parent);
    ~CSettings() override = default;

    void readSettings(QObject *control = nullptr);
    void setTranslationEngine(CStructures::TranslationEngine engine);
    void updateXapianIndexDirs(const QStringList& directories);
    QString getSelectedLangPair(CStructures::TranslationEngine engine) const;
    CStructures::TranslationEngine getTranslationEngineFromName(const QString& name, bool *ok) const;

    void setupXapianTimerInterval(QObject *control, int secs);
    int getXapianTimerInterval();
    void setMangaBackgroundColor(const QColor &color);

Q_SIGNALS:
    void adblockRulesUpdated();
    void mangaViewerSettingsUpdated();

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
