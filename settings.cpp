#include <QSettings>
#include <QDir>
#include <QApplication>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QMessageBox>
#include <QtConcurrentRun>
#include <QDebug>
#include <goldendictlib/goldendictmgr.hh>
#include "settings.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "globalprivate.h"
#include "settingstab.h"
#include "miniqxt/qxtglobalshortcut.h"
#include "genericfuncs.h"
#include "snviewer.h"
#include "userscript.h"
#include "bookmarks.h"

const QVector<QColor> CSettings::snippetColors = {
    QColor(Qt::red), QColor(Qt::green), QColor(Qt::blue), QColor(Qt::cyan),
    QColor(Qt::magenta), QColor(Qt::darkRed), QColor(Qt::darkGreen),
    QColor(Qt::darkBlue), QColor(Qt::darkCyan), QColor(Qt::darkMagenta),
    QColor(Qt::darkYellow), QColor(Qt::gray) };

const QUrl::FormattingOptions CSettings::adblockUrlFmt = QUrl::RemoveUserInfo
                                                         | QUrl::RemovePort
                                                         | QUrl::RemoveFragment
                                                         | QUrl::StripTrailingSlash;

namespace CDefaults {
const quint32 bigdataMagic = 0x4a500001;
const quint32 bigdataMagicMask = 0xffff0000;
const int delayedLoadingAdblock = 1500;
}

CSettings::CSettings(QObject *parent)
    : QObject(parent)
{
    fontFixed=QString::fromUtf8(CDefaults::fontFixed);
    fontSerif=QString::fromUtf8(CDefaults::fontSerif);
    fontSansSerif=QString::fromUtf8(CDefaults::fontSansSerif);

    savedAuxDir=QDir::homePath();
    savedAuxSaveDir=savedAuxDir;
    overrideTransFont=QApplication::font();
    fontStandard=QApplication::font().family();

#if WITH_RECOLL
    searchEngine = CStructures::seRecoll;
#elif WITH_BALOO5
    searchEngine = CStructures::seBaloo5;
#else
    searchEngine = CStructures::seNone;
#endif

    auto g = qobject_cast<CGlobalControl *>(parent);

    connect(&(g->d_func()->settingsSaveTimer), &QTimer::timeout, this, &CSettings::writeSettings);
    g->d_func()->settingsSaveTimer.start();
}

void CSettings::writeSettings()
{
    if (!gSet) return;
    if (!gSet->d_func()->settingsSaveMutex.tryLock()) return;

    QSettings settings(QSL("kernel1024"), QSL("jpreader"));
    settings.beginGroup(QSL("MainWindow"));
    settings.remove(QString());

    QFileInfo fi(settings.fileName());
    writeBinaryBigData(fi.dir().filePath(QSL("jpreader-bigdata.bin")));

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
    settings.setValue(QSL("useOverrideTransFont"),gSet->m_ui->useOverrideTransFont());
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
    settings.setValue(QSL("forceTransFontColor"),gSet->m_ui->forceFontColor());
    settings.setValue(QSL("forcedTransFontColor"),forcedFontColor.name());
    settings.setValue(QSL("gctxHotkey"),gSet->m_ui->gctxTranHotkey.shortcut().toString());

    settings.setValue(QSL("searchEngine"),static_cast<int>(searchEngine));
    settings.setValue(QSL("atlTcpRetryCount"),atlTcpRetryCount);
    settings.setValue(QSL("atlTcpTimeout"),atlTcpTimeout);
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
    settings.setValue(QSL("createCoredumps"),createCoredumps);
    settings.setValue(QSL("overrideUserAgent"),overrideUserAgent);
    settings.setValue(QSL("userAgent"),userAgent);
    settings.setValue(QSL("ignoreSSLErrors"),ignoreSSLErrors);
    settings.setValue(QSL("showFavicons"),gSet->d_func()->webProfile->settings()->
                      testAttribute(QWebEngineSettings::AutoLoadIconsForPage));
    settings.setValue(QSL("diskCacheSize"),gSet->d_func()->webProfile->httpCacheMaximumSize());
    settings.setValue(QSL("jsLogConsole"),jsLogConsole);
    settings.setValue(QSL("dontUseNativeFileDialog"),dontUseNativeFileDialog);
    settings.setValue(QSL("defaultSearchEngine"),defaultSearchEngine);

    settings.setValue(QSL("pdfExtractImages"),pdfExtractImages);
    settings.setValue(QSL("pdfImageMaxSize"),pdfImageMaxSize);
    settings.setValue(QSL("pdfImageQuality"),pdfImageQuality);

    settings.setValue(QSL("pixivFetchImages"),pixivFetchImages);

    settings.endGroup();
    gSet->d_func()->settingsSaveMutex.unlock();
}

bool CSettings::readBinaryBigData(QObject *control, const QString& filename)
{
    auto g = qobject_cast<CGlobalControl *>(control);
    if (!g) return false;

    QFile fBigdata(filename);
    if (!fBigdata.open(QIODevice::ReadOnly))
        return false;

    QDataStream bigdata(&fBigdata);
    quint32 magic;
    bigdata >> magic;
    if ((magic & CDefaults::bigdataMagicMask) !=
            (CDefaults::bigdataMagic & CDefaults::bigdataMagicMask)) {
        qWarning() << "Broken file format with config bigdata section: " << filename;
        fBigdata.close();
        return false;
    }
    bigdata.setVersion(QDataStream::Qt_5_10);

    bigdata >> g->d_func()->searchHistory;
    Q_EMIT g->updateAllQueryLists();

    bigdata >> atlHostHistory;
    bigdata >> g->d_func()->mainHistory;
    bigdata >> userAgentHistory;
    bigdata >> dictPaths;
    bigdata >> g->d_func()->ctxSearchEngines;
    bigdata >> atlCerts;
    bigdata >> g->d_func()->recentFiles;

    CStringHash scripts;
    bigdata >> scripts;
    g->initUserScripts(scripts);

    QByteArray bookmarks;
    bigdata >> bookmarks;
    g->d_func()->bookmarksManager->load(bookmarks);

    bigdata >> translatorPairs;
    bigdata >> subsentencesMode;

    QByteArray adblock;
    bigdata >> adblock;
    QtConcurrent::run([this,g,adblock]() {
        QDataStream bufs(adblock);
        bufs >> g->d_func()->adblock;
        qInfo() << "Adblock rules loaded";
        Q_EMIT adblockRulesUpdated();
    });
    bigdata >> g->d_func()->noScriptWhiteList;

    fBigdata.close();
    return true;
}

void CSettings::writeBinaryBigData(const QString &filename)
{
    QFile fBigdata(filename);
    if (!fBigdata.open(QIODevice::WriteOnly)) {
        qCritical() << "Unable to create config file for bigdata section: " << filename;
        return;
    }

    QDataStream bigdata(&fBigdata);
    bigdata << CDefaults::bigdataMagic;
    bigdata.setVersion(QDataStream::Qt_5_10);

    bigdata << gSet->d_func()->searchHistory;
    bigdata << atlHostHistory;
    bigdata << gSet->d_func()->mainHistory;
    bigdata << userAgentHistory;
    bigdata << dictPaths;
    bigdata << gSet->d_func()->ctxSearchEngines;
    bigdata << atlCerts;
    bigdata << gSet->d_func()->recentFiles;
    bigdata << gSet->getUserScripts();
    bigdata << gSet->d_func()->bookmarksManager->save();
    bigdata << translatorPairs;
    bigdata << subsentencesMode;

    QByteArray buf;
    QDataStream bufs(&buf,QIODevice::WriteOnly);
    bufs << gSet->d_func()->adblock;
    bigdata << buf;

    bigdata << gSet->d_func()->noScriptWhiteList;

    fBigdata.close();
}

void CSettings::readSettings(QObject *control)
{
    auto g = qobject_cast<CGlobalControl *>(control);
    if (!g) return;

    QSettings settings(QSL("kernel1024"), QSL("jpreader"));
    settings.beginGroup(QSL("MainWindow"));

    QFileInfo fi(settings.fileName());
    if (!readBinaryBigData(g,fi.dir().filePath(QSL("jpreader-bigdata.bin")))) {
        QSettings bigdata(QSL("kernel1024"), QSL("jpreader-bigdata"));
        bigdata.beginGroup(QSL("main"));

        g->d_func()->searchHistory = bigdata.value(QSL("searchHistory"),QStringList()).toStringList();
        Q_EMIT g->updateAllQueryLists();

        atlHostHistory = bigdata.value(QSL("atlHostHistory"),QStringList()).toStringList();
        g->d_func()->mainHistory = bigdata.value(QSL("history")).value<CUrlHolderVector>();
        userAgentHistory = bigdata.value(QSL("userAgentHistory"),QStringList()).toStringList();
        dictPaths = bigdata.value(QSL("dictPaths"),QStringList()).toStringList();
        g->d_func()->ctxSearchEngines = bigdata.value(QSL("ctxSearchEngines")).value<CStringHash>();
        atlCerts = bigdata.value(QSL("atlasCertificates")).value<CSslCertificateHash>();
        g->d_func()->recentFiles = bigdata.value(QSL("recentFiles"),QStringList()).toStringList();
        g->initUserScripts(bigdata.value(QSL("userScripts")).value<CStringHash>());
        g->d_func()->bookmarksManager->load(bigdata.value(QSL("bookmarks2"),QByteArray()).toByteArray());
        translatorPairs = bigdata.value(QSL("translatorPairs")).value<CLangPairVector>();
        subsentencesMode = bigdata.value(QSL("subsentencesMode")).value<CSubsentencesMode>();
        g->d_func()->adblock = bigdata.value(QSL("adblock")).value<CAdBlockVector>();
        g->d_func()->noScriptWhiteList = bigdata.value(QSL("noScriptWhitelist")).value<CStringSet>();

        bigdata.endGroup();
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
    bool jsstate = settings.value(QSL("javascript"),true).toBool();
    g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,jsstate);
    g->m_ui->actionJSUsage->setChecked(jsstate);
    g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadImages,
                                         settings.value(QSL("autoloadimages"),true).toBool());
    g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,
                                         settings.value(QSL("enablePlugins"),false).toBool());

    useAdblock=settings.value(QSL("useAdblock"),CDefaults::useAdblock).toBool();
    useNoScript=settings.value(QSL("useNoScript"),CDefaults::useNoScript).toBool();
    g->m_ui->actionOverrideTransFont->setChecked(settings.value(QSL("useOverrideTransFont"),
                                                        false).toBool());
    overrideTransFont.setFamily(settings.value(QSL("overrideTransFont"),"Verdana").toString());
    overrideTransFont.setPointSize(settings.value(QSL("overrideTransFontSize"),
                                             CDefaults::overrideTransFontSize).toInt());
    overrideStdFonts=settings.value(QSL("overrideStdFonts"),
                                    CDefaults::overrideStdFonts).toBool();
    fontStandard=settings.value(QSL("standardFont"),QApplication::font().family()).toString();
    fontFixed=settings.value(QSL("fixedFont"),CDefaults::fontFixed).toString();
    fontSerif=settings.value(QSL("serifFont"),CDefaults::fontSerif).toString();
    fontSansSerif=settings.value(QSL("sansSerifFont"),CDefaults::fontSansSerif).toString();
    overrideFontSizes=settings.value(QSL("overrideFontSizes"),CDefaults::overrideFontSizes).toBool();
    fontSizeMinimal=settings.value(QSL("fontSizeMinimal"),CDefaults::fontSizeMinimal).toInt();
    fontSizeDefault=settings.value(QSL("fontSizeDefault"),CDefaults::fontSizeDefault).toInt();
    fontSizeFixed=settings.value(QSL("fontSizeFixed"),CDefaults::fontSizeFixed).toInt();
    g->m_ui->actionOverrideTransFontColor->setChecked(settings.value(
                                                  QSL("forceTransFontColor"),false).toBool());
    forcedFontColor=QColor(settings.value(
                               QSL("forcedTransFontColor"),"#000000").toString());
    QString hk = settings.value(QSL("gctxHotkey"),QString()).toString();
    if (!hk.isEmpty()) {
        g->m_ui->gctxTranHotkey.setShortcut(QKeySequence::fromString(hk));
        if (!g->m_ui->gctxTranHotkey.shortcut().isEmpty())
            g->m_ui->gctxTranHotkey.setEnabled();
    }
    searchEngine = static_cast<CStructures::SearchEngine>(settings.value(
                                                 QSL("searchEngine"),
                                                 CDefaults::searchEngine).toInt());
    atlTcpRetryCount = settings.value(QSL("atlTcpRetryCount"),
                                      CDefaults::atlTcpRetryCount).toInt();
    atlTcpTimeout = settings.value(QSL("atlTcpTimeout"),
                                   CDefaults::atlTcpTimeout).toInt();
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
    jsLogConsole = settings.value(QSL("jsLogConsole"),CDefaults::jsLogConsole).toBool();
    dontUseNativeFileDialog = settings.value(QSL("dontUseNativeFileDialog"),
                                             CDefaults::dontUseNativeFileDialog).toBool();
    createCoredumps = settings.value(QSL("createCoredumps"),
                                     CDefaults::createCoredumps).toBool();
    ignoreSSLErrors = settings.value(QSL("ignoreSSLErrors"),
                                     CDefaults::ignoreSSLErrors).toBool();
    g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,
                                            settings.value(QSL("showFavicons"),true).toBool());
    defaultSearchEngine = settings.value(QSL("defaultSearchEngine"),QString()).toString();
    g->d_func()->webProfile->setHttpCacheMaximumSize(settings.value(QSL("diskCacheSize"),0).toInt());

    pdfExtractImages = settings.value(QSL("pdfExtractImages"),
                                      CDefaults::pdfExtractImages).toBool();
    pdfImageMaxSize = settings.value(QSL("pdfImageMaxSize"),
                                     CDefaults::pdfImageMaxSize).toInt();
    pdfImageQuality = settings.value(QSL("pdfImageQuality"),
                                     CDefaults::pdfImageQuality).toInt();

    pixivFetchImages = settings.value(QSL("pixivFetchImages"),
                                      CDefaults::pixivFetchImages).toBool();

    overrideUserAgent=settings.value(QSL("overrideUserAgent"),
                                     CDefaults::overrideUserAgent).toBool();
    userAgent=settings.value(QSL("userAgent"),QString()).toString();
    if (userAgent.isEmpty()) {
        overrideUserAgent=false;
    } else if (userAgentHistory.isEmpty()) {
        userAgentHistory << userAgent;
    }

    if (overrideUserAgent)
        g->d_func()->webProfile->setHttpUserAgent(userAgent);


    settings.endGroup();

    Q_EMIT g->updateAllBookmarks();
    g->updateProxyWithMenuUpdate(proxyUse,true);
    g->m_ui->rebuildLanguageActions(g);
}

void CSettings::settingsDlg()
{
    auto dlg = CSettingsTab::instance();
    if (dlg!=nullptr) {
        dlg->activateWindow();
        dlg->setTabFocused();
    }
}

void CSettings::setTranslationEngine(CStructures::TranslationEngine engine)
{
    translatorEngine = engine;
    Q_EMIT gSet->translationEngineChanged();
}

void CSettings::checkRestoreLoad(CMainWindow *w)
{
    if (gSet->d_func()->restoreLoadChecked) return;
    gSet->d_func()->restoreLoadChecked = true;

    QVector<QUrl> urls;
    urls.clear();
    QSettings settings(QSL("kernel1024"), QSL("jpreader-tabs"));
    settings.beginGroup(QSL("OpenedTabs"));
    int cnt = settings.value(QSL("tabsCnt"), 0).toInt();
    for (int i=0;i<cnt;i++) {
        QUrl u = settings.value(QSL("tab_%1").arg(i),QUrl()).toUrl();
        if (u.isValid() && !u.isEmpty())
            urls << u;
    }
    settings.endGroup();

    if (!urls.isEmpty()) {
        if (QMessageBox::question(w,tr("JPReader"),
                                  tr("Program crashed in previous run. Restore all tabs?"),
                                  QMessageBox::Yes,QMessageBox::No) == QMessageBox::Yes) {
            for (int i=0;i<cnt;i++)
                new CSnippetViewer(w,urls.at(i));
        }
    }
}

QVector<QUrl> CSettings::getTabsList() const
{
    QVector<QUrl> res;
    if (!gSet) return res;

    for (int i=0;i<gSet->d_func()->mainWindows.count();i++) {
        for (int j=0;j<gSet->d_func()->mainWindows.at(i)->tabMain->count();j++) {
            auto sn = qobject_cast<CSnippetViewer *>(gSet->d_func()->mainWindows.at(i)->tabMain->widget(j));
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
    QSettings tabs(QSL("kernel1024"), QSL("jpreader-tabs"));
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
