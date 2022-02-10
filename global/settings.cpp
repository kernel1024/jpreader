#include <QSettings>
#include <QDir>
#include <QApplication>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QMessageBox>
#include <QThread>
#include <QDebug>

#include "settings.h"
#include "mainwindow.h"
#include "control.h"
#include "control_p.h"
#include "history.h"
#include "browserfuncs.h"
#include "network.h"
#include "ui.h"
#include "startup.h"
#include "utils/settingstab.h"
#include "miniqxt/qxtglobalshortcut.h"
#include "utils/genericfuncs.h"
#include "browser/browser.h"
#include "browser-utils/userscript.h"
#include "browser-utils/bookmarks.h"

namespace CMaterialColors {
const unsigned int red400 = 0xef5350;
const unsigned int pink400 = 0xec407a;

const unsigned int purple400 = 0xab47bc;
const unsigned int deepPurple400 = 0x7e57c2;
const unsigned int indigo400 = 0x5c6bc0;

const unsigned int blue400 = 0x42a5f5;
const unsigned int lightBlue400 = 0x29b6f6;
const unsigned int cyan400 = 0x26c6da;
const unsigned int teal400 = 0x26a69a;

const unsigned int green400 = 0x66bb6a;
const unsigned int lightGreen400 = 0x9ccc65;
const unsigned int lime400 = 0xd4e157;

const unsigned int yellow400 = 0xffee58;
const unsigned int amber400 = 0xffca28;
const unsigned int orange400 = 0xffa726;
const unsigned int deepOrange400 = 0xff7043;

const unsigned int brown400 = 0x8d6e63;
const unsigned int gray400 = 0xbdbdbd;
const unsigned int blueGray400 = 0x78909c;

const QVector<QColor> contrastColors = {
    QColor(red400),
    QColor(blue400),
    QColor(purple400),
    QColor(green400),
    QColor(amber400),
    QColor(gray400),
    QColor(pink400),
    QColor(indigo400),
    QColor(teal400),
    QColor(lime400),
    QColor(deepOrange400),
    QColor(blueGray400),
    QColor(deepPurple400),
    QColor(cyan400),
    QColor(lightGreen400),
    QColor(orange400),
    QColor(brown400),
    QColor(lightBlue400),
    QColor(yellow400)
};
}

const QVector<QColor> CSettings::graphColors = CMaterialColors::contrastColors;

const QVector<QColor> CSettings::snippetColors = CMaterialColors::contrastColors;

const QUrl::FormattingOptions CSettings::adblockUrlFmt = QUrl::RemoveUserInfo
                                                         | QUrl::RemovePort
                                                         | QUrl::RemoveFragment
                                                         | QUrl::StripTrailingSlash;

CSettings::CSettings(CGlobalControl *parent)
    : QObject(parent)
{
    fontFixed=QString::fromUtf8(CDefaults::fontFixed);
    fontSerif=QString::fromUtf8(CDefaults::fontSerif);
    fontSansSerif=QString::fromUtf8(CDefaults::fontSansSerif);

    savedAuxDir=QDir::homePath();
    savedAuxSaveDir=savedAuxDir;

#if WITH_XAPIAN
    searchEngine = CStructures::seXapian;
#elif WITH_RECOLL
    searchEngine = CStructures::seRecoll;
#elif WITH_BALOO5
    searchEngine = CStructures::seBaloo5;
#else
    searchEngine = CStructures::seNone;
#endif

    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        overrideTransFont=QApplication::font();
        fontStandard=QApplication::font().family();

        connect(&(parent->d_func()->settingsSaveTimer), &QTimer::timeout, this, &CSettings::writeSettings);
        parent->d_func()->settingsSaveTimer.start();
    }
}

void CSettings::writeSettings()
{
    if (!gSet) return;
    if (gSet->actions() == nullptr) return;
    if (!gSet->d_func()->settingsSaveMutex.tryLock()) return;

    QSettings settings;
    settings.beginGroup(QSL("MainWindow"));
    settings.remove(QString());

    QFileInfo fi(settings.fileName());
    writeBinaryBigData(fi.dir().filePath(QSL("jpreader-bigdata")));

    settings.setValue(QSL("maxLimit"),maxSearchLimit);
    settings.setValue(QSL("maxHistory"),maxHistory);
    settings.setValue(QSL("maxRecent"),maxRecent);
    settings.setValue(QSL("maxAdblockWhiteList"),maxAdblockWhiteList);
    settings.setValue(QSL("browser"),sysBrowser);
    settings.setValue(QSL("editor"),sysEditor);
    settings.setValue(QSL("tr_engine"),translatorEngine);
    settings.setValue(QSL("atlasHost"),atlHost);
    settings.setValue(QSL("atlasPort"),atlPort);
    settings.setValue(QSL("atlasToken"),atlToken);
    settings.setValue(QSL("atlasProto"),static_cast<int>(atlProto));
    settings.setValue(QSL("auxDir"),savedAuxDir);
    settings.setValue(QSL("auxSaveDir"),savedAuxSaveDir);
    settings.setValue(QSL("emptyRestore"),emptyRestore);
    settings.setValue(QSL("javascript"),gSet->d_func()->webProfile->settings()->
                      testAttribute(QWebEngineSettings::JavascriptEnabled));
    settings.setValue(QSL("autoloadimages"),gSet->d_func()->webProfile->settings()->
                      testAttribute(QWebEngineSettings::AutoLoadImages));
    settings.setValue(QSL("enablePlugins"),gSet->d_func()->webProfile->settings()->
                      testAttribute(QWebEngineSettings::PluginsEnabled));
    settings.setValue(QSL("recycledCount"),maxRecycled);

    settings.setValue(QSL("useAdblock"),useAdblock);
    settings.setValue(QSL("useNoScript"),useNoScript);
    settings.setValue(QSL("useOverrideTransFont"),gSet->m_actions->useOverrideTransFont());
    settings.setValue(QSL("overrideTransFont"),overrideTransFont.family());
    settings.setValue(QSL("overrideTransFontSize"),overrideTransFont.pointSize());
    settings.setValue(QSL("overrideStdFonts"),overrideStdFonts);
    settings.setValue(QSL("standardFont"),fontStandard);
    settings.setValue(QSL("fixedFont"),fontFixed);
    settings.setValue(QSL("serifFont"),fontSerif);
    settings.setValue(QSL("sansSerifFont"),fontSansSerif);
    settings.setValue(QSL("overrideFontSizes"),overrideFontSizes);
    settings.setValue(QSL("fontSizeMinimal"),fontSizeMinimal);
    settings.setValue(QSL("fontSizeDefault"),fontSizeDefault);
    settings.setValue(QSL("fontSizeFixed"),fontSizeFixed);
    settings.setValue(QSL("forceTransFontColor"),gSet->m_actions->forceFontColor());
    settings.setValue(QSL("forcedTransFontColor"),forcedFontColor.name());
    settings.setValue(QSL("gctxHotkey"),gctxSequence.toString());
    settings.setValue(QSL("autofillHotkey"),autofillSequence.toString());

    settings.setValue(QSL("searchEngine"),static_cast<int>(searchEngine));
    settings.setValue(QSL("translatorRetryCount"),translatorRetryCount);
    settings.setValue(QSL("showTabCloseButtons"),showTabCloseButtons);
    settings.setValue(QSL("proxyHost"),proxyHost);
    settings.setValue(QSL("proxyPort"),proxyPort);
    settings.setValue(QSL("proxyLogin"),proxyLogin);
    settings.setValue(QSL("proxyPassword"),proxyPassword);
    settings.setValue(QSL("proxyUse"),proxyUse);
    settings.setValue(QSL("proxyType"),proxyType);
    settings.setValue(QSL("proxyUseTranslator"),proxyUseTranslator);
    settings.setValue(QSL("bingKey"),bingKey);
    settings.setValue(QSL("yandexKey"),yandexKey);
    settings.setValue(QSL("awsRegion"),awsRegion);
    settings.setValue(QSL("awsAccessKey"),awsAccessKey);
    settings.setValue(QSL("awsSecretKey"),awsSecretKey);
    settings.setValue(QSL("yandexCloudApiKey"),yandexCloudApiKey);
    settings.setValue(QSL("yandexCloudFolderID"),yandexCloudFolderID);
    settings.setValue(QSL("gcpJsonKeyFile"),gcpJsonKeyFile);
    settings.setValue(QSL("aliAccessKeyID"),aliAccessKeyID);
    settings.setValue(QSL("aliAccessKeySecret"),aliAccessKeySecret);
    settings.setValue(QSL("aliCloudTranslatorMode"),static_cast<int>(aliCloudTranslatorMode));
    settings.setValue(QSL("createCoredumps"),createCoredumps);
    settings.setValue(QSL("overrideUserAgent"),overrideUserAgent);
    settings.setValue(QSL("userAgent"),userAgent);
    settings.setValue(QSL("ignoreSSLErrors"),ignoreSSLErrors);
    settings.setValue(QSL("showFavicons"),gSet->d_func()->webProfile->settings()->
                      testAttribute(QWebEngineSettings::AutoLoadIconsForPage));
    settings.setValue(QSL("diskCacheSize"),gSet->d_func()->webProfile->httpCacheMaximumSize());
    settings.setValue(QSL("jsLogConsole"),jsLogConsole);
    settings.setValue(QSL("downloaderCleanCompleted"),downloaderCleanCompleted);
    settings.setValue(QSL("dontUseNativeFileDialog"),dontUseNativeFileDialog);
    settings.setValue(QSL("defaultSearchEngine"),defaultSearchEngine);

    settings.setValue(QSL("pdfExtractImages"),pdfExtractImages);
    settings.setValue(QSL("pdfImageMaxSize"),pdfImageMaxSize);
    settings.setValue(QSL("pdfImageQuality"),pdfImageQuality);

    settings.setValue(QSL("pixivFetchImages"),pixivFetchImages);
    settings.setValue(QSL("pixivFetchCovers"),static_cast<int>(pixivFetchCovers));
    settings.setValue(QSL("pixivMangaPageSize"),static_cast<int>(pixivMangaPageSize));

    settings.setValue(QSL("translatorCacheEnabled"),translatorCacheEnabled);
    settings.setValue(QSL("translatorCacheSize"),translatorCacheSize);

    settings.setValue(QSL("xapianStemmerLang"),xapianStemmerLang);
    settings.setValue(QSL("xapianStartDelay"),getXapianTimerInterval());
    settings.setValue(QSL("xapianIndexDirList"),xapianIndexDirList);

    settings.setValue(QSL("domWorkerReplyTimeoutSec"),domWorkerReplyTimeoutSec);

    settings.setValue(QSL("mangaCacheWidth"),mangaCacheWidth);
    settings.setValue(QSL("mangaMagnifySize"),mangaMagnifySize);
    settings.setValue(QSL("mangaScrollDelta"),mangaScrollDelta);
    settings.setValue(QSL("mangaScrollFactor"),mangaScrollFactor);
    settings.setValue(QSL("upscaleFilter"),static_cast<int>(mangaUpscaleFilter));
    settings.setValue(QSL("downscaleFilter"),static_cast<int>(mangaDownscaleFilter));
    settings.setValue(QSL("mangaResizeBlur"),mangaResizeBlur);
    settings.setValue(QSL("mangaBackgroundColor"),mangaBackgroundColor.name());
    settings.setValue(QSL("mangaUseFineRendering"),mangaUseFineRendering);

    settings.endGroup();
    gSet->d_func()->settingsSaveMutex.unlock();
}

bool CSettings::readBinaryBigData(QObject *control, const QString& dirname)
{
    auto *g = qobject_cast<CGlobalControl *>(control);
    if (!g) return false;

    QDir bigdataDir(dirname);
    if (!bigdataDir.exists()) {
        return false;
    }

    sslTrustedInvalidCerts = readData(bigdataDir,QSL("sslTrustedInvalidCerts")).value<CSslCertificateHash>();

    if (g->m_actions.isNull()) return true; // CLI mode

    atlHostHistory = readData(bigdataDir,QSL("atlHostHistory"),QStringList()).toStringList();
    g->d_func()->mainHistory = readData(bigdataDir,QSL("mainHistory")).value<CUrlHolderVector>();
    userAgentHistory = readData(bigdataDir,QSL("userAgentHistory")).toStringList();
    dictPaths = readData(bigdataDir,QSL("dictPaths")).toStringList();
    g->d_func()->ctxSearchEngines = readData(bigdataDir,QSL("ctxSearchEngines")).value<CStringHash>();
    g->d_func()->recentFiles = readData(bigdataDir,QSL("recentFiles")).toStringList();
    translatorPairs = readData(bigdataDir,QSL("translatorPairs")).value<QVector<CLangPair> >();
    g->m_actions->setSubsentencesModeHash(readData(bigdataDir,QSL("subsentencesMode")).value<CSubsentencesMode>());
    g->d_func()->noScriptWhiteList = readData(bigdataDir,QSL("noScriptWhiteList")).value<CStringSet>();
    translatorStatistics = readData(bigdataDir,QSL("translatorStatistics")).value<CTranslatorStatistics>();
    selectedLangPairs = readData(bigdataDir,QSL("selectedLangPairs")).value<CSelectedLangPairs>();
    g->d_func()->pixivKeywordsHistory = readData(bigdataDir,QSL("pixivKeywordHistory")).toStringList();

    g->d_func()->searchHistory = readData(bigdataDir,QSL("searchHistory")).toStringList();
    Q_EMIT g->m_history->updateAllQueryLists();

    auto scripts = readData(bigdataDir,QSL("userScripts")).value<CStringHash>();
    g->m_browser->initUserScripts(scripts);

    const QByteArray bookmarks = readByteArray(bigdataDir,QSL("bookmarks"));
    g->d_func()->bookmarksManager->load(bookmarks);

    const QByteArray adblock = readByteArray(bigdataDir,QSL("adblock"));
    QThread* th = QThread::create([this,g,adblock]() {
        QDataStream bufs(adblock);
        CAdBlockVector rules;
        QVariant vcount;
        bufs >> vcount;
        if (vcount.isValid() && vcount.canConvert<qulonglong>()) {
            qulonglong count = vcount.toULongLong();
            rules.reserve(count);
            for (qulonglong i=0; i<count; i++) {
                CAdBlockRule rule;
                bufs >> rule;
                rules.push_back(rule);
            }
            g->d_func()->adblockModifyMutex.lock();
            g->d_func()->adblock = rules;
            g->d_func()->adblockModifyMutex.unlock();
            qInfo() << "Adblock rules loaded";
            Q_EMIT adblockRulesUpdated();
        }
    });
    connect(th,&QThread::finished,th,&QThread::deleteLater);
    th->setObjectName(QSL("AdblockLoader"));
    th->start();

    return true;
}

void CSettings::writeBinaryBigData(const QString &dirname)
{
    QDir bigdataDir(dirname);
    if (!bigdataDir.exists()) {
        if (!bigdataDir.mkpath(QSL("."))) {
            qCritical() << "Unable to create config file directory for bigdata section: " << dirname;
            return;
        }
    }

    if (!writeData(dirname,QSL("searchHistory"),    QVariant::fromValue(gSet->d_func()->searchHistory)))
        qCritical() << "Unable to save searchHistory.";
    if (!writeData(dirname,QSL("atlHostHistory"),   QVariant::fromValue(atlHostHistory)))
        qCritical() << "Unable to save atlHostHistory.";
    if (!writeData(dirname,QSL("mainHistory"),      QVariant::fromValue(gSet->d_func()->mainHistory)))
        qCritical() << "Unable to save mainHistory.";
    if (!writeData(dirname,QSL("userAgentHistory"), QVariant::fromValue(userAgentHistory)))
        qCritical() << "Unable to save userAgentHistory.";
    if (!writeData(dirname,QSL("dictPaths"),        QVariant::fromValue(dictPaths)))
        qCritical() << "Unable to save dictPaths.";
    if (!writeData(dirname,QSL("ctxSearchEngines"), QVariant::fromValue(gSet->d_func()->ctxSearchEngines)))
        qCritical() << "Unable to save ctxSearchEngines.";
    if (!writeData(dirname,QSL("sslTrustedInvalidCerts"), QVariant::fromValue(sslTrustedInvalidCerts)))
        qCritical() << "Unable to save sslTrustedInvalidCerts.";
    if (!writeData(dirname,QSL("recentFiles"),      QVariant::fromValue(gSet->d_func()->recentFiles)))
        qCritical() << "Unable to save recentFiles.";
    if (!writeData(dirname,QSL("userScripts"),      QVariant::fromValue(gSet->m_browser->getUserScripts())))
        qCritical() << "Unable to save userScripts.";
    if (!writeData(dirname,QSL("translatorPairs"),  QVariant::fromValue(translatorPairs)))
        qCritical() << "Unable to save translatorPairs.";
    if (!writeData(dirname,QSL("subsentencesMode"), QVariant::fromValue(gSet->m_actions->getSubsentencesModeHash())))
        qCritical() << "Unable to save subsentencesMode.";
    if (!writeData(dirname,QSL("noScriptWhiteList"),QVariant::fromValue(gSet->d_func()->noScriptWhiteList)))
        qCritical() << "Unable to save noScriptWhiteList.";
    if (!writeData(dirname,QSL("translatorStatistics"),QVariant::fromValue(translatorStatistics)))
        qCritical() << "Unable to save translatorStatistics.";
    if (!writeData(dirname,QSL("selectedLangPairs"),QVariant::fromValue(selectedLangPairs)))
        qCritical() << "Unable to save selectedLangPairs.";
    if (!writeData(dirname,QSL("pixivKeywordHistory"), QVariant::fromValue(gSet->d_func()->pixivKeywordsHistory)))
        qCritical() << "Unable to save pixivKeywordHistory.";

    if (!writeByteArray(dirname,QSL("bookmarks"),   gSet->d_func()->bookmarksManager->save()))
        qCritical() << "Unable to save bookmarks.";

    // save adblock data as binary dump for bulk loading with deferred parsing
    QByteArray buf;
    QDataStream bufs(&buf,QIODevice::WriteOnly);
    qulonglong count = gSet->d_func()->adblock.size();
    bufs << QVariant::fromValue(count);
    for (const auto &rule : gSet->d_func()->adblock)
        bufs << rule;
    if (!writeByteArray(dirname,QSL("adblock"),buf))
        qCritical() << "Unable to save adblock.";
}

QByteArray CSettings::readByteArray(const QDir& directory, const QString &name)
{
    QByteArray res;
    QFile fBigdata(directory.filePath(name));
    if (!fBigdata.open(QIODevice::ReadOnly)) {
        return res;
    }
    res = fBigdata.readAll();
    fBigdata.close();
    return res;
}

QVariant CSettings::readData(const QDir& directory, const QString &name, const QVariant &defaultValue)
{
    QByteArray ba = readByteArray(directory,name);
    if (ba.isEmpty())
        return defaultValue;

    QVariant buf;
    QDataStream bigdata(ba);
    bigdata.setVersion(QDataStream::Qt_5_10);
    bigdata >> buf;

    if (!defaultValue.isNull() && defaultValue.isValid()) {
        if (defaultValue.userType() == buf.userType())
            return buf;
    }

    if (buf.isValid())
        return buf;

    qCritical() << "Failed to read valid value for " << name;
    return defaultValue;
}

bool CSettings::writeByteArray(const QDir& directory, const QString &name, const QByteArray &data)
{
    QFile fBigdata(directory.filePath(name));
    if (!fBigdata.open(QIODevice::WriteOnly)) {
        qCritical() << "Unable to create config file for bigdata section: " << fBigdata.fileName();
        return false;
    }
    fBigdata.write(data);
    fBigdata.close();
    return true;
}

bool CSettings::writeData(const QDir& directory, const QString &name, const QVariant &data)
{
    QByteArray ba;
    QDataStream bigdata(&ba,QIODevice::WriteOnly);
    bigdata.setVersion(QDataStream::Qt_5_10);
    bigdata << data;
    return writeByteArray(directory,name,ba);
}

void CSettings::readSettings(QObject *control)
{
    auto *g = qobject_cast<CGlobalControl *>(control);
    Q_ASSERT(g!=nullptr);

    QSettings settings;
    settings.beginGroup(QSL("MainWindow"));

    QFileInfo fi(settings.fileName());

    if (!readBinaryBigData(g,fi.dir().filePath(QSL("jpreader-bigdata")))) {
        qCritical() << "Unable to read configuration bigdata.";
    }

    int idx=0;
    while (idx<g->d_func()->mainHistory.count()) {
        if (g->d_func()->mainHistory.at(idx).url.toString().startsWith(QSL("data:"),
                                                                       Qt::CaseInsensitive)) {
            g->d_func()->mainHistory.removeAt(idx);
        } else {
            idx++;
        }
    }

    maxSearchLimit = settings.value(QSL("maxLimit"),CDefaults::maxSearchLimit).toInt();
    maxHistory = settings.value(QSL("maxHistory"),CDefaults::maxHistory).toInt();
    maxRecent = settings.value(QSL("maxRecent"),CDefaults::maxRecent).toInt();
    maxAdblockWhiteList = settings.value(QSL("maxAdblockWhiteList"),
                                         CDefaults::maxAdblockWhiteList).toInt();
    sysBrowser = settings.value(QSL("browser"),CDefaults::sysBrowser).toString();
    sysEditor = settings.value(QSL("editor"),CDefaults::sysEditor).toString();
    previousTranslatorEngine = translatorEngine;
    translatorEngine = static_cast<CStructures::TranslationEngine>(
                           settings.value(QSL("tr_engine"),CDefaults::translatorEngine).toInt());
    atlHost = settings.value(QSL("atlasHost"),"localhost").toString();
    atlPort = static_cast<quint16>(settings.value(QSL("atlasPort"),CDefaults::atlPort).toUInt());
    atlToken = settings.value(QSL("atlasToken"),QString()).toString();
    atlProto = static_cast<QSsl::SslProtocol>(settings.value(QSL("atlasProto"),
                                                             CDefaults::atlProto).toInt());
    savedAuxDir = settings.value(QSL("auxDir"),QDir::homePath()).toString();
    savedAuxSaveDir = settings.value(QSL("auxSaveDir"),QDir::homePath()).toString();
    emptyRestore = settings.value(QSL("emptyRestore"),CDefaults::emptyRestore).toBool();
    maxRecycled = settings.value(QSL("recycledCount"),CDefaults::maxRecycled).toInt();

    useAdblock=settings.value(QSL("useAdblock"),CDefaults::useAdblock).toBool();
    useNoScript=settings.value(QSL("useNoScript"),CDefaults::useNoScript).toBool();
    overrideTransFont.setFamily(settings.value(QSL("overrideTransFont"),"Verdana").toString());
    overrideTransFont.setPointSize(settings.value(QSL("overrideTransFontSize"),
                                             CDefaults::overrideTransFontSize).toInt());
    overrideStdFonts=settings.value(QSL("overrideStdFonts"),
                                    CDefaults::overrideStdFonts).toBool();
    fontStandard=settings.value(QSL("standardFont"),CDefaults::fontSansSerif).toString();
    fontFixed=settings.value(QSL("fixedFont"),CDefaults::fontFixed).toString();
    fontSerif=settings.value(QSL("serifFont"),CDefaults::fontSerif).toString();
    fontSansSerif=settings.value(QSL("sansSerifFont"),CDefaults::fontSansSerif).toString();
    overrideFontSizes=settings.value(QSL("overrideFontSizes"),CDefaults::overrideFontSizes).toBool();
    fontSizeMinimal=settings.value(QSL("fontSizeMinimal"),CDefaults::fontSizeMinimal).toInt();
    fontSizeDefault=settings.value(QSL("fontSizeDefault"),CDefaults::fontSizeDefault).toInt();
    fontSizeFixed=settings.value(QSL("fontSizeFixed"),CDefaults::fontSizeFixed).toInt();
    forcedFontColor=QColor(settings.value(
                               QSL("forcedTransFontColor"),"#000000").toString());
    searchEngine = static_cast<CStructures::SearchEngine>(settings.value(
                                                 QSL("searchEngine"),
                                                 CDefaults::searchEngine).toInt());
    translatorRetryCount = settings.value(QSL("translatorRetryCount"),
                                          CDefaults::translatorRetryCount).toInt();
    showTabCloseButtons = settings.value(QSL("showTabCloseButtons"),
                                         CDefaults::showTabCloseButtons).toBool();
    proxyHost = settings.value(QSL("proxyHost"),QString()).toString();
    proxyPort = static_cast<quint16>(settings.value(QSL("proxyPort"),
                                                    CDefaults::proxyPort).toUInt());
    proxyType = static_cast<QNetworkProxy::ProxyType>(
                    settings.value(QSL("proxyType"),
                                   QNetworkProxy::HttpCachingProxy).toInt());
    proxyLogin = settings.value(QSL("proxyLogin"),QString()).toString();
    proxyPassword = settings.value(QSL("proxyPassword"),QString()).toString();
    proxyUse = settings.value(QSL("proxyUse"),CDefaults::proxyUse).toBool();
    proxyUseTranslator = settings.value(QSL("proxyUseTranslator"),
                                        CDefaults::proxyUseTranslator).toBool();
    bingKey = settings.value(QSL("bingKey"),QString()).toString();
    yandexKey = settings.value(QSL("yandexKey"),QString()).toString();
    awsRegion = settings.value(QSL("awsRegion"),QString()).toString();
    awsAccessKey = settings.value(QSL("awsAccessKey"),QString()).toString();
    awsSecretKey = settings.value(QSL("awsSecretKey"),QString()).toString();
    yandexCloudApiKey = settings.value(QSL("yandexCloudApiKey"),QString()).toString();
    yandexCloudFolderID = settings.value(QSL("yandexCloudFolderID"),QString()).toString();
    gcpJsonKeyFile = settings.value(QSL("gcpJsonKeyFile"),QString()).toString();
    aliAccessKeyID = settings.value(QSL("aliAccessKeyID"),QString()).toString();
    aliAccessKeySecret = settings.value(QSL("aliAccessKeySecret"),QString()).toString();
    aliCloudTranslatorMode = static_cast<CStructures::AliCloudTranslatorMode>(
                                 settings.value(QSL("aliCloudTranslatorMode"),0).toInt());

    jsLogConsole = settings.value(QSL("jsLogConsole"),CDefaults::jsLogConsole).toBool();
    downloaderCleanCompleted = settings.value(QSL("downloaderCleanCompleted"),
                                              CDefaults::downloaderCleanCompleted).toBool();
    dontUseNativeFileDialog = settings.value(QSL("dontUseNativeFileDialog"),
                                             CDefaults::dontUseNativeFileDialog).toBool();
    createCoredumps = settings.value(QSL("createCoredumps"),
                                     CDefaults::createCoredumps).toBool();
    ignoreSSLErrors = settings.value(QSL("ignoreSSLErrors"),
                                     CDefaults::ignoreSSLErrors).toBool();
    defaultSearchEngine = settings.value(QSL("defaultSearchEngine"),QString()).toString();

    pdfExtractImages = settings.value(QSL("pdfExtractImages"),
                                      CDefaults::pdfExtractImages).toBool();
    pdfImageMaxSize = settings.value(QSL("pdfImageMaxSize"),
                                     CDefaults::pdfImageMaxSize).toInt();
    pdfImageQuality = settings.value(QSL("pdfImageQuality"),
                                     CDefaults::pdfImageQuality).toInt();

    pixivFetchImages = settings.value(QSL("pixivFetchImages"),
                                      CDefaults::pixivFetchImages).toBool();
    pixivFetchCovers = static_cast<CStructures::PixivFetchCoversMode>(settings.value(QSL("pixivFetchCovers"),
                                      CDefaults::pixivFetchCovers).toInt());
    pixivMangaPageSize = static_cast<CStructures::PixivMangaPageSize>(settings.value(QSL("pixivMangaPageSize"),
                                      CDefaults::pixivMangaPageSize).toInt());

    translatorCacheEnabled = settings.value(QSL("translatorCacheEnabled"),
                                            CDefaults::translatorCacheEnabled).toBool();
    translatorCacheSize = settings.value(QSL("translatorCacheSize"),
                                         CDefaults::translatorCacheSize).toInt();

    domWorkerReplyTimeoutSec = settings.value(QSL("domWorkerReplyTimeoutSec"),
                                              CDefaults::domWorkerReplyTimeoutSec).toInt();

    setupXapianTimerInterval(g,settings.value(QSL("xapianStartDelay"),CDefaults::xapianStartDelay).toInt());
    xapianStemmerLang = settings.value(QSL("xapianStemmerLang"),QString()).toString();
    xapianIndexDirList = settings.value(QSL("xapianIndexDirList"),QStringList()).toStringList();

    overrideUserAgent=settings.value(QSL("overrideUserAgent"),
                                     CDefaults::overrideUserAgent).toBool();
    userAgent=settings.value(QSL("userAgent"),QString()).toString();
    if (userAgent.isEmpty()) {
        overrideUserAgent=false;
    } else if (userAgentHistory.isEmpty()) {
        userAgentHistory << userAgent;
    }

    mangaCacheWidth = settings.value(QSL("mangaCacheWidth"),CDefaults::mangaCacheWidth).toInt();
    mangaMagnifySize = settings.value(QSL("mangaMagnifySize"),CDefaults::mangaMagnifySize).toInt();
    mangaScrollDelta = settings.value(QSL("mangaScrollDelta"),CDefaults::mangaScrollDelta).toInt();
    mangaScrollFactor = settings.value(QSL("mangaScrollFactor"),CDefaults::mangaScrollFactor).toInt();
    mangaUpscaleFilter = static_cast<Blitz::ScaleFilterType>(
                             settings.value(QSL("upscaleFilter"),Blitz::MitchellFilter).toInt());
    mangaDownscaleFilter = static_cast<Blitz::ScaleFilterType>(
                               settings.value(QSL("downscaleFilter"),Blitz::LanczosFilter).toInt());
    mangaResizeBlur = settings.value(QSL("mangaResizeBlur"),CDefaults::mangaResizeBlur).toDouble();
    mangaBackgroundColor = QColor(settings.value(QSL("mangaBackgroundColor"),
                                                 CDefaults::mangaBackgroundColor).toString());
    mangaUseFineRendering = settings.value(QSL("mangaUseFineRendering"),CDefaults::mangaUseFineRendering).toBool();

    if (g->m_actions) { // GUI mode

        bool jsstate = settings.value(QSL("javascript"),true).toBool();
        g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,jsstate);
        g->m_actions->actionJSUsage->setChecked(jsstate);
        g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadImages,
                                             settings.value(QSL("autoloadimages"),true).toBool());
        g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,
                                             settings.value(QSL("enablePlugins"),false).toBool());
        g->m_actions->actionOverrideTransFont->setChecked(settings.value(QSL("useOverrideTransFont"),
                                                            false).toBool());

        g->m_actions->actionOverrideTransFontColor->setChecked(settings.value(
                                                      QSL("forceTransFontColor"),false).toBool());

        gctxSequence = QKeySequence::fromString(settings.value(QSL("gctxHotkey"),QString()).toString());
        autofillSequence = QKeySequence::fromString(settings.value(QSL("autofillHotkey"),QString()).toString());

        g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,
                                                settings.value(QSL("showFavicons"),true).toBool());
        g->d_func()->webProfile->setHttpCacheMaximumSize(settings.value(QSL("diskCacheSize"),0).toInt());

        if (overrideUserAgent)
            g->d_func()->webProfile->setHttpUserAgent(userAgent);
    }

    settings.endGroup();

    g->m_net->updateProxyWithMenuUpdate(proxyUse,true);
    if (g->m_actions) { // GUI mode
        Q_EMIT g->m_history->updateAllBookmarks();

        g->m_actions->rebuildLanguageActions(g);
        g->m_actions->rebindGctxHotkey(g);
        g->d_func()->reloadXapianFilesystemWatcher(g);
    }
}

void CSettings::settingsTab()
{
    auto *dlg = CSettingsTab::instance();
    if (dlg!=nullptr) {
        dlg->activateWindow();
        dlg->setTabFocused();
    }
}

void CSettings::setTranslationEngine(CStructures::TranslationEngine engine)
{
    selectedLangPairs[translatorEngine] = gSet->m_actions->getActiveLangPair();
    previousTranslatorEngine = translatorEngine;
    translatorEngine = engine;
    if (selectedLangPairs.contains(engine))
        gSet->m_actions->setActiveLangPair(selectedLangPairs.value(engine));

    Q_EMIT gSet->m_ui->translationEngineChanged();
}

void CSettings::updateXapianIndexDirs(const QStringList &directories)
{
    xapianIndexDirList = directories;
    gSet->d_func()->reloadXapianFilesystemWatcher();
    gSet->m_startup->startupXapianIndexerDirect(false,false);
}

QString CSettings::getSelectedLangPair(CStructures::TranslationEngine engine) const
{
    return selectedLangPairs.value(engine);
}

CStructures::TranslationEngine CSettings::getTranslationEngineFromName(const QString &name, bool *ok) const
{
    CStructures::TranslationEngine res = gSet->settings()->translatorEngine;
    (*ok) = false;
    if (!name.isEmpty()) {
        for (auto it = CStructures::translationEngineCodes().constBegin(),
             end = CStructures::translationEngineCodes().constEnd(); it != end; ++it) {
            if (name == it.value()) {
                res = it.key();
                (*ok) = true;
                break;
            }
        }
    }
    return res;
}

void CSettings::setupXapianTimerInterval(QObject* control, int secs)
{
    auto *g = qobject_cast<CGlobalControl *>(control);
    Q_ASSERT(g!=nullptr);

    int xapianSec = secs;
    if (xapianSec<5) xapianSec = 5;
    g->d_func()->xapianIndexerTimer.stop();
    g->d_func()->xapianIndexerTimer.setSingleShot(true);
    g->d_func()->xapianIndexerTimer.setInterval(xapianSec*CDefaults::oneK);
}

int CSettings::getXapianTimerInterval()
{
    return gSet->d_func()->xapianIndexerTimer.interval() / CDefaults::oneK;
}

void CSettings::setMangaBackgroundColor(const QColor &color)
{
    mangaBackgroundColor = color;
    Q_EMIT mangaViewerSettingsUpdated();
}

void CSettings::checkRestoreLoad(CMainWindow *w)
{
    if (gSet->d_func()->restoreLoadChecked) return;
    gSet->d_func()->restoreLoadChecked = true;

    QVector<QUrl> urls;
    QSettings settings(QCoreApplication::organizationName(), QSL("jpreader-tabs"));
    settings.beginGroup(QSL("OpenedTabs"));
    int cnt = settings.value(QSL("tabsCnt"), 0).toInt();
    urls.reserve(cnt);
    for (int i=0;i<cnt;i++) {
        QUrl u = settings.value(QSL("tab_%1").arg(i),QUrl()).toUrl();
        if (u.isValid() && !u.isEmpty())
            urls << u;
    }
    settings.endGroup();

    if (!urls.isEmpty()) {
        if (QMessageBox::question(w,QGuiApplication::applicationDisplayName(),
                                  tr("Program crashed in previous run. Restore all tabs?"),
                                  QMessageBox::Yes,QMessageBox::No) == QMessageBox::Yes) {
            for (int i=0;i<cnt;i++)
                new CBrowserTab(w,urls.at(i));
        }
    }
}

QVector<QUrl> CSettings::getTabsList() const
{
    QVector<QUrl> res;
    if (!gSet) return res;

    for (int i=0;i<gSet->d_func()->mainWindows.count();i++) {
        for (int j=0;j<gSet->d_func()->mainWindows.at(i)->tabMain->count();j++) {
            auto *sn = qobject_cast<CBrowserTab *>(gSet->d_func()->mainWindows.at(i)->tabMain->widget(j));
            if (!sn) continue;
            QUrl url = sn->getUrl();
            if (url.isValid() && !url.isEmpty())
                res << url;
        }
    }

    return res;
}

void CSettings::writeTabsListPrivate(const QVector<QUrl>& tabList)
{
    QSettings tabs(QCoreApplication::organizationName(), QSL("jpreader-tabs"));
    tabs.beginGroup(QSL("OpenedTabs"));
    tabs.remove(QString());
    tabs.setValue(QSL("tabsCnt"), tabList.count());
    for (int i=0;i<tabList.count();i++)
        tabs.setValue(QSL("tab_%1").arg(i),tabList.at(i));
    tabs.endGroup();
}

void CSettings::clearTabsList()
{
    QVector<QUrl> tabs;
    writeTabsListPrivate(tabs);
}

void CSettings::writeTabsList()
{
    writeTabsListPrivate(getTabsList());
}
