#include <QSettings>
#include <QDir>
#include <QApplication>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QMessageBox>
#include <goldendictlib/goldendictmgr.hh>
#include "settings.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "globalprivate.h"
#include "settingsdlg.h"
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

CSettings::CSettings(QObject *parent)
    : QObject(parent)
{
    fontFixed=QString::fromUtf8(CSettingsDefault::fontFixed);
    fontSerif=QString::fromUtf8(CSettingsDefault::fontSerif);
    fontSansSerif=QString::fromUtf8(CSettingsDefault::fontSansSerif);

    savedAuxDir=QDir::homePath();
    savedAuxSaveDir=savedAuxDir;
    overrideFont=QApplication::font();
    fontStandard=QApplication::font().family();

#if WITH_RECOLL
    searchEngine = seRecoll;
#elif WITH_BALOO5
    searchEngine = seBaloo5;
#else
    searchEngine = seNone;
#endif

    auto g = qobject_cast<CGlobalControl *>(parent);

    connect(&(g->d_func()->settingsSaveTimer), &QTimer::timeout, this, &CSettings::writeSettings);
    g->d_func()->settingsSaveTimer.start();
}

void CSettings::writeSettings()
{
    if (!gSet) return;
    if (gSet->d_func()->settingsSaveMutex.tryLock()) return;

    QSettings settings(QStringLiteral("kernel1024"), QStringLiteral("jpreader"));
    QSettings bigdata(QStringLiteral("kernel1024"), QStringLiteral("jpreader-bigdata"));
    settings.beginGroup(QStringLiteral("MainWindow"));
    bigdata.beginGroup(QStringLiteral("main"));
    settings.remove(QString());
    bigdata.remove(QString());

    bigdata.setValue(QStringLiteral("searchHistory"),QVariant::fromValue(gSet->d_func()->searchHistory));
    bigdata.setValue(QStringLiteral("history"),QVariant::fromValue(gSet->d_func()->mainHistory));
    bigdata.setValue(QStringLiteral("adblock"),QVariant::fromValue(gSet->d_func()->adblock));
    bigdata.setValue(QStringLiteral("noScriptWhitelist"),QVariant::fromValue(gSet->d_func()->noScriptWhiteList));
    bigdata.setValue(QStringLiteral("atlHostHistory"),QVariant::fromValue(atlHostHistory));
    bigdata.setValue(QStringLiteral("scpHostHistory"),QVariant::fromValue(scpHostHistory));
    bigdata.setValue(QStringLiteral("userAgentHistory"),QVariant::fromValue(userAgentHistory));
    bigdata.setValue(QStringLiteral("dictPaths"),QVariant::fromValue(dictPaths));
    bigdata.setValue(QStringLiteral("ctxSearchEngines"),QVariant::fromValue(gSet->d_func()->ctxSearchEngines));
    bigdata.setValue(QStringLiteral("atlasCertificates"),QVariant::fromValue(atlCerts));
    bigdata.setValue(QStringLiteral("recentFiles"),QVariant::fromValue(gSet->d_func()->recentFiles));
    bigdata.setValue(QStringLiteral("userScripts"),QVariant::fromValue(gSet->getUserScripts()));
    bigdata.setValue(QStringLiteral("bookmarks2"),QVariant::fromValue(gSet->d_func()->bookmarksManager->save()));
    bigdata.setValue(QStringLiteral("translatorPairs"),QVariant::fromValue(translatorPairs));

    settings.setValue(QStringLiteral("hostingDir"),hostingDir);
    settings.setValue(QStringLiteral("hostingUrl"),hostingUrl);
    settings.setValue(QStringLiteral("maxLimit"),maxSearchLimit);
    settings.setValue(QStringLiteral("maxHistory"),maxHistory);
    settings.setValue(QStringLiteral("maxRecent"),maxRecent);
    settings.setValue(QStringLiteral("maxAdblockWhiteList"),maxAdblockWhiteList);
    settings.setValue(QStringLiteral("browser"),sysBrowser);
    settings.setValue(QStringLiteral("editor"),sysEditor);
    settings.setValue(QStringLiteral("tr_engine"),translatorEngine);
    settings.setValue(QStringLiteral("scp"),useScp);
    settings.setValue(QStringLiteral("scphost"),scpHost);
    settings.setValue(QStringLiteral("scpparams"),scpParams);
    settings.setValue(QStringLiteral("atlasHost"),atlHost);
    settings.setValue(QStringLiteral("atlasPort"),atlPort);
    settings.setValue(QStringLiteral("atlasToken"),atlToken);
    settings.setValue(QStringLiteral("atlasProto"),static_cast<int>(atlProto));
    settings.setValue(QStringLiteral("auxDir"),savedAuxDir);
    settings.setValue(QStringLiteral("auxSaveDir"),savedAuxSaveDir);
    settings.setValue(QStringLiteral("emptyRestore"),emptyRestore);
    settings.setValue(QStringLiteral("javascript"),gSet->d_func()->webProfile->settings()->
                      testAttribute(QWebEngineSettings::JavascriptEnabled));
    settings.setValue(QStringLiteral("autoloadimages"),gSet->d_func()->webProfile->settings()->
                      testAttribute(QWebEngineSettings::AutoLoadImages));
    settings.setValue(QStringLiteral("enablePlugins"),gSet->d_func()->webProfile->settings()->
                      testAttribute(QWebEngineSettings::PluginsEnabled));
    settings.setValue(QStringLiteral("recycledCount"),maxRecycled);

    settings.setValue(QStringLiteral("useAdblock"),useAdblock);
    settings.setValue(QStringLiteral("useNoScript"),useNoScript);
    settings.setValue(QStringLiteral("useOverrideFont"),gSet->m_ui->useOverrideFont());
    settings.setValue(QStringLiteral("overrideFont"),overrideFont.family());
    settings.setValue(QStringLiteral("overrideFontSize"),overrideFont.pointSize());
    settings.setValue(QStringLiteral("overrideStdFonts"),overrideStdFonts);
    settings.setValue(QStringLiteral("standardFont"),fontStandard);
    settings.setValue(QStringLiteral("fixedFont"),fontFixed);
    settings.setValue(QStringLiteral("serifFont"),fontSerif);
    settings.setValue(QStringLiteral("sansSerifFont"),fontSansSerif);
    settings.setValue(QStringLiteral("forceFontColor"),gSet->m_ui->forceFontColor());
    settings.setValue(QStringLiteral("forcedFontColor"),forcedFontColor.name());
    settings.setValue(QStringLiteral("gctxHotkey"),gSet->m_ui->gctxTranHotkey.shortcut().toString());
    settings.setValue(QStringLiteral("translateSubSentences"),gSet->m_ui->translateSubSentences());

    settings.setValue(QStringLiteral("searchEngine"),static_cast<int>(searchEngine));
    settings.setValue(QStringLiteral("atlTcpRetryCount"),atlTcpRetryCount);
    settings.setValue(QStringLiteral("atlTcpTimeout"),atlTcpTimeout);
    settings.setValue(QStringLiteral("showTabCloseButtons"),showTabCloseButtons);
    settings.setValue(QStringLiteral("proxyHost"),proxyHost);
    settings.setValue(QStringLiteral("proxyPort"),proxyPort);
    settings.setValue(QStringLiteral("proxyLogin"),proxyLogin);
    settings.setValue(QStringLiteral("proxyPassword"),proxyPassword);
    settings.setValue(QStringLiteral("proxyUse"),proxyUse);
    settings.setValue(QStringLiteral("proxyType"),proxyType);
    settings.setValue(QStringLiteral("proxyUseTranslator"),proxyUseTranslator);
    settings.setValue(QStringLiteral("bingKey"),bingKey);
    settings.setValue(QStringLiteral("yandexKey"),yandexKey);
    settings.setValue(QStringLiteral("createCoredumps"),createCoredumps);
    settings.setValue(QStringLiteral("overrideUserAgent"),overrideUserAgent);
    settings.setValue(QStringLiteral("userAgent"),userAgent);
    settings.setValue(QStringLiteral("ignoreSSLErrors"),ignoreSSLErrors);
    settings.setValue(QStringLiteral("showFavicons"),gSet->d_func()->webProfile->settings()->
                      testAttribute(QWebEngineSettings::AutoLoadIconsForPage));
    settings.setValue(QStringLiteral("diskCacheSize"),gSet->d_func()->webProfile->httpCacheMaximumSize());
    settings.setValue(QStringLiteral("jsLogConsole"),jsLogConsole);
    settings.setValue(QStringLiteral("dontUseNativeFileDialog"),dontUseNativeFileDialog);
    settings.setValue(QStringLiteral("defaultSearchEngine"),defaultSearchEngine);
    settings.setValue(QStringLiteral("settingsDlgSize"),gSet->d_func()->settingsDlgSize);

    settings.setValue(QStringLiteral("pdfExtractImages"),pdfExtractImages);
    settings.setValue(QStringLiteral("pdfImageMaxSize"),pdfImageMaxSize);
    settings.setValue(QStringLiteral("pdfImageQuality"),pdfImageQuality);

    settings.setValue(QStringLiteral("pixivFetchImages"),pixivFetchImages);

    settings.endGroup();
    bigdata.endGroup();
    gSet->d_func()->settingsSaveMutex.unlock();
}

void CSettings::readSettings(QObject *control)
{
    auto g = qobject_cast<CGlobalControl *>(control);
    if (!g) return;

    const QSize defaultSettingsDlgSize(850,380);

    QSettings settings(QStringLiteral("kernel1024"), QStringLiteral("jpreader"));
    QSettings bigdata(QStringLiteral("kernel1024"), QStringLiteral("jpreader-bigdata"));
    settings.beginGroup(QStringLiteral("MainWindow"));
    bigdata.beginGroup(QStringLiteral("main"));

    g->d_func()->searchHistory = bigdata.value(QStringLiteral("searchHistory"),QStringList()).toStringList();

    Q_EMIT g->updateAllQueryLists();

    atlHostHistory = bigdata.value(QStringLiteral("atlHostHistory"),QStringList()).toStringList();
    scpHostHistory = bigdata.value(QStringLiteral("scpHostHistory"),QStringList()).toStringList();
    g->d_func()->mainHistory = bigdata.value(QStringLiteral("history")).value<CUrlHolderVector>();
    userAgentHistory = bigdata.value(QStringLiteral("userAgentHistory"),QStringList()).toStringList();
    dictPaths = bigdata.value(QStringLiteral("dictPaths"),QStringList()).toStringList();
    g->d_func()->ctxSearchEngines = bigdata.value(QStringLiteral("ctxSearchEngines")).value<CStringHash>();
    atlCerts = bigdata.value(QStringLiteral("atlasCertificates")).value<CSslCertificateHash>();
    g->d_func()->recentFiles = bigdata.value(QStringLiteral("recentFiles"),QStringList()).toStringList();
    g->initUserScripts(bigdata.value(QStringLiteral("userScripts")).value<CStringHash>());
    g->d_func()->bookmarksManager->load(bigdata.value(QStringLiteral("bookmarks2"),QByteArray()).toByteArray());
    translatorPairs = bigdata.value(QStringLiteral("translatorPairs")).value<CLangPairVector>();

    int idx=0;
    while (idx<g->d_func()->mainHistory.count()) {
        if (g->d_func()->mainHistory.at(idx).url.toString().startsWith(QStringLiteral("data:"),
                                                                       Qt::CaseInsensitive)) {
            g->d_func()->mainHistory.removeAt(idx);
        } else {
            idx++;
        }
    }

    g->d_func()->adblock = bigdata.value(QStringLiteral("adblock")).value<CAdBlockVector>();
    g->d_func()->noScriptWhiteList = bigdata.value(QStringLiteral("noScriptWhitelist")).value<CStringSet>();

    hostingDir = settings.value(QStringLiteral("hostingDir"),QString()).toString();
    hostingUrl = settings.value(QStringLiteral("hostingUrl"),"about:blank").toString();
    maxSearchLimit = settings.value(QStringLiteral("maxLimit"),CSettingsDefault::maxSearchLimit).toInt();
    maxHistory = settings.value(QStringLiteral("maxHistory"),CSettingsDefault::maxHistory).toInt();
    maxRecent = settings.value(QStringLiteral("maxRecent"),CSettingsDefault::maxRecent).toInt();
    maxAdblockWhiteList = settings.value(QStringLiteral("maxAdblockWhiteList"),
                                         CSettingsDefault::maxAdblockWhiteList).toInt();
    sysBrowser = settings.value(QStringLiteral("browser"),CSettingsDefault::sysBrowser).toString();
    sysEditor = settings.value(QStringLiteral("editor"),CSettingsDefault::sysEditor).toString();
    translatorEngine = static_cast<TranslationEngine>(
                           settings.value(QStringLiteral("tr_engine"),CSettingsDefault::translatorEngine).toInt());
    useScp = settings.value(QStringLiteral("scp"),CSettingsDefault::useScp).toBool();
    scpHost = settings.value(QStringLiteral("scphost"),QString()).toString();
    scpParams = settings.value(QStringLiteral("scpparams"),QString()).toString();
    atlHost = settings.value(QStringLiteral("atlasHost"),"localhost").toString();
    atlPort = static_cast<quint16>(settings.value(QStringLiteral("atlasPort"),CSettingsDefault::atlPort).toUInt());
    atlToken = settings.value(QStringLiteral("atlasToken"),QString()).toString();
    atlProto = static_cast<QSsl::SslProtocol>(settings.value(QStringLiteral("atlasProto"),
                                                             CSettingsDefault::atlProto).toInt());
    savedAuxDir = settings.value(QStringLiteral("auxDir"),QDir::homePath()).toString();
    savedAuxSaveDir = settings.value(QStringLiteral("auxSaveDir"),QDir::homePath()).toString();
    emptyRestore = settings.value(QStringLiteral("emptyRestore"),CSettingsDefault::emptyRestore).toBool();
    maxRecycled = settings.value(QStringLiteral("recycledCount"),CSettingsDefault::maxRecycled).toInt();
    bool jsstate = settings.value(QStringLiteral("javascript"),true).toBool();
    g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,jsstate);
    g->m_ui->actionJSUsage->setChecked(jsstate);
    g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadImages,
                                         settings.value(QStringLiteral("autoloadimages"),true).toBool());
    g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,
                                         settings.value(QStringLiteral("enablePlugins"),false).toBool());

    useAdblock=settings.value(QStringLiteral("useAdblock"),CSettingsDefault::useAdblock).toBool();
    useNoScript=settings.value(QStringLiteral("useNoScript"),CSettingsDefault::useNoScript).toBool();
    g->m_ui->actionOverrideFont->setChecked(settings.value(QStringLiteral("useOverrideFont"),
                                                        false).toBool());
    overrideFont.setFamily(settings.value(QStringLiteral("overrideFont"),"Verdana").toString());
    overrideFont.setPointSize(settings.value(QStringLiteral("overrideFontSize"),
                                             CSettingsDefault::overrideFontSize).toInt());
    overrideStdFonts=settings.value(QStringLiteral("overrideStdFonts"),
                                    CSettingsDefault::overrideStdFonts).toBool();
    fontStandard=settings.value(QStringLiteral("standardFont"),QApplication::font().family()).toString();
    fontFixed=settings.value(QStringLiteral("fixedFont"),CSettingsDefault::fontFixed).toString();
    fontSerif=settings.value(QStringLiteral("serifFont"),CSettingsDefault::fontSerif).toString();
    fontSansSerif=settings.value(QStringLiteral("sansSerifFont"),CSettingsDefault::fontSansSerif).toString();
    g->m_ui->actionOverrideFontColor->setChecked(settings.value(
                                                  QStringLiteral("forceFontColor"),false).toBool());
    forcedFontColor=QColor(settings.value(
                               QStringLiteral("forcedFontColor"),"#000000").toString());
    QString hk = settings.value(QStringLiteral("gctxHotkey"),QString()).toString();
    g->m_ui->actionTranslateSubSentences->setChecked(settings.value(
                                                      QStringLiteral("translateSubSentences"),
                                                      false).toBool());
    if (!hk.isEmpty()) {
        g->m_ui->gctxTranHotkey.setShortcut(QKeySequence::fromString(hk));
        if (!g->m_ui->gctxTranHotkey.shortcut().isEmpty())
            g->m_ui->gctxTranHotkey.setEnabled();
    }
    searchEngine = static_cast<SearchEngine>(settings.value(
                                                 QStringLiteral("searchEngine"),
                                                 CSettingsDefault::searchEngine).toInt());
    atlTcpRetryCount = settings.value(QStringLiteral("atlTcpRetryCount"),
                                      CSettingsDefault::atlTcpRetryCount).toInt();
    atlTcpTimeout = settings.value(QStringLiteral("atlTcpTimeout"),
                                   CSettingsDefault::atlTcpTimeout).toInt();
    showTabCloseButtons = settings.value(QStringLiteral("showTabCloseButtons"),
                                         CSettingsDefault::showTabCloseButtons).toBool();
    proxyHost = settings.value(QStringLiteral("proxyHost"),QString()).toString();
    proxyPort = static_cast<quint16>(settings.value(QStringLiteral("proxyPort"),
                                                    CSettingsDefault::proxyPort).toUInt());
    proxyType = static_cast<QNetworkProxy::ProxyType>(
                    settings.value(QStringLiteral("proxyType"),
                                   QNetworkProxy::HttpCachingProxy).toInt());
    proxyLogin = settings.value(QStringLiteral("proxyLogin"),QString()).toString();
    proxyPassword = settings.value(QStringLiteral("proxyPassword"),QString()).toString();
    proxyUse = settings.value(QStringLiteral("proxyUse"),CSettingsDefault::proxyUse).toBool();
    proxyUseTranslator = settings.value(QStringLiteral("proxyUseTranslator"),
                                        CSettingsDefault::proxyUseTranslator).toBool();
    bingKey = settings.value(QStringLiteral("bingKey"),QString()).toString();
    yandexKey = settings.value(QStringLiteral("yandexKey"),QString()).toString();
    jsLogConsole = settings.value(QStringLiteral("jsLogConsole"),CSettingsDefault::jsLogConsole).toBool();
    dontUseNativeFileDialog = settings.value(QStringLiteral("dontUseNativeFileDialog"),
                                             CSettingsDefault::dontUseNativeFileDialog).toBool();
    createCoredumps = settings.value(QStringLiteral("createCoredumps"),
                                     CSettingsDefault::createCoredumps).toBool();
    ignoreSSLErrors = settings.value(QStringLiteral("ignoreSSLErrors"),
                                     CSettingsDefault::ignoreSSLErrors).toBool();
    g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,
                                            settings.value(QStringLiteral("showFavicons"),true).toBool());
    defaultSearchEngine = settings.value(QStringLiteral("defaultSearchEngine"),QString()).toString();
    g->d_func()->webProfile->setHttpCacheMaximumSize(settings.value(QStringLiteral("diskCacheSize"),0).toInt());
    g->d_func()->settingsDlgSize=settings.value(QStringLiteral("settingsDlgSize"),defaultSettingsDlgSize).toSize();

    pdfExtractImages = settings.value(QStringLiteral("pdfExtractImages"),
                                      CSettingsDefault::pdfExtractImages).toBool();
    pdfImageMaxSize = settings.value(QStringLiteral("pdfImageMaxSize"),
                                     CSettingsDefault::pdfImageMaxSize).toInt();
    pdfImageQuality = settings.value(QStringLiteral("pdfImageQuality"),
                                     CSettingsDefault::pdfImageQuality).toInt();

    pixivFetchImages = settings.value(QStringLiteral("pixivFetchImages"),
                                      CSettingsDefault::pixivFetchImages).toBool();

    overrideUserAgent=settings.value(QStringLiteral("overrideUserAgent"),
                                     CSettingsDefault::overrideUserAgent).toBool();
    userAgent=settings.value(QStringLiteral("userAgent"),QString()).toString();
    if (userAgent.isEmpty()) {
        overrideUserAgent=false;
    } else if (userAgentHistory.isEmpty()) {
        userAgentHistory << userAgent;
    }

    if (overrideUserAgent)
        g->d_func()->webProfile->setHttpUserAgent(userAgent);


    settings.endGroup();
    bigdata.endGroup();
    if (!hostingDir.endsWith('/')) hostingDir.append('/');
    if (!hostingUrl.endsWith('/')) hostingUrl.append('/');

    Q_EMIT g->updateAllBookmarks();
    g->updateProxyWithMenuUpdate(proxyUse,true);
    g->m_ui->rebuildLanguageActions(g);
}

void CSettings::settingsDlg()
{
    if (!gSet) return;
    auto dlg = gSet->d_func()->settingsDialog;
    if (dlg!=nullptr) {
        dlg->activateWindow();
        return;
    }
    dlg = new CSettingsDlg();
    gSet->d_func()->settingsDialog = dlg;

    if (!scpHostHistory.contains(scpHost))
        scpHostHistory.append(scpHost);
    if (!atlHostHistory.contains(atlHost))
        atlHostHistory.append(atlHost);

    dlg->loadFromGlobal();

    dlg->setMainHistory(gSet->d_func()->mainHistory);
    dlg->setQueryHistory(gSet->d_func()->searchHistory);
    dlg->setAdblock(gSet->d_func()->adblock);
    dlg->setNoScriptWhitelist(gSet->d_func()->noScriptWhiteList);
    dlg->setUserScripts(gSet->getUserScripts());

    dlg->resize(gSet->d_func()->settingsDlgSize);

    if (dlg->exec()==QDialog::Accepted) {
        QStringList dPaths;
        dlg->saveToGlobal(dPaths);

        if (!gSet->m_settings->hostingDir.endsWith('/')) gSet->m_settings->hostingDir.append('/');
        if (!gSet->m_settings->hostingUrl.endsWith('/')) gSet->m_settings->hostingUrl.append('/');

        gSet->d_func()->searchHistory.clear();
        gSet->d_func()->searchHistory.append(dlg->getQueryHistory());

        gSet->d_func()->adblock.clear();
        gSet->d_func()->adblock.append(dlg->getAdblock());
        gSet->d_func()->adblockWhiteListMutex.lock();
        gSet->d_func()->adblockWhiteList.clear();
        gSet->d_func()->adblockWhiteListMutex.unlock();
        gSet->initUserScripts(dlg->getUserScripts());

        gSet->d_func()->noScriptWhiteList = dlg->getNoScriptWhitelist();

        if (scpHostHistory.contains(scpHost)) {
            scpHostHistory.move(scpHostHistory.indexOf(scpHost),0);
        } else {
            scpHostHistory.prepend(scpHost);
        }
        if (atlHostHistory.contains(atlHost)) {
            atlHostHistory.move(atlHostHistory.indexOf(atlHost),0);
        } else {
            atlHostHistory.prepend(atlHost);
        }

        translatorPairs = dlg->getLangPairList();
        gSet->m_ui->rebuildLanguageActions();

        forcedFontColor=dlg->getOverridedFontColor();

        if (!gSet->m_ui->gctxTranHotkey.shortcut().isEmpty())
            gSet->m_ui->gctxTranHotkey.setEnabled();

        if (!userAgentHistory.contains(userAgent))
            userAgentHistory << userAgent;

        if (userAgent.isEmpty())
            overrideUserAgent=false;
        if (overrideUserAgent)
            gSet->webProfile()->setHttpUserAgent(userAgent);

        gSet->d_func()->ctxSearchEngines = dlg->getSearchEngines();

        if (compareStringLists(dictPaths,dPaths)!=0) {
            dictPaths.clear();
            dictPaths.append(dPaths);
            gSet->d_func()->dictManager->loadDictionaries(dictPaths, dictIndexDir);
        }

        gSet->updateProxyWithMenuUpdate(proxyUse,true);

        Q_EMIT gSet->updateAllQueryLists();
        Q_EMIT gSet->settingsUpdated();

        gSet->d_func()->settingsDlgSize = dlg->size();
    }

    connect(dlg,&CSettingsDlg::destroyed,gSet->d_func(),&CGlobalControlPrivate::settingsDialogDestroyed);
    dlg->deleteLater();
}

void CSettings::setTranslationEngine(TranslationEngine engine)
{
    translatorEngine = engine;
    Q_EMIT gSet->settingsUpdated();
}

void CSettings::checkRestoreLoad(CMainWindow *w)
{
    if (gSet->d_func()->restoreLoadChecked) return;
    gSet->d_func()->restoreLoadChecked = true;

    QVector<QUrl> urls;
    urls.clear();
    QSettings settings(QStringLiteral("kernel1024"), QStringLiteral("jpreader-tabs"));
    settings.beginGroup(QStringLiteral("OpenedTabs"));
    int cnt = settings.value(QStringLiteral("tabsCnt"), 0).toInt();
    for (int i=0;i<cnt;i++) {
        QUrl u = settings.value(QStringLiteral("tab_%1").arg(i),QUrl()).toUrl();
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
    QSettings tabs(QStringLiteral("kernel1024"), QStringLiteral("jpreader-tabs"));
    tabs.beginGroup(QStringLiteral("OpenedTabs"));
    tabs.remove(QString());
    tabs.setValue(QStringLiteral("tabsCnt"), tabList.count());
    for (int i=0;i<tabList.count();i++)
        tabs.setValue(QStringLiteral("tab_%1").arg(i),tabList.at(i));
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
