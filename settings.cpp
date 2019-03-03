#include <QSettings>
#include <QDir>
#include <QApplication>
#include <QWebEngineSettings>
#include <QMessageBox>
#include <goldendictlib/goldendictmgr.hh>
#include "settings.h"
#include "mainwindow.h"
#include "globalcontrol.h"
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

CSettings::CSettings(QObject *parent)
    : QObject(parent),
      maxHistory(5000),
      maxRecent(10),
      maxSearchLimit(1000),
      translatorEngine(TE_ATLAS),
      maxRecycled(20),
      atlTcpRetryCount(3),
      atlTcpTimeout(2),
      maxAdblockWhiteList(5000),
      pdfImageMaxSize(250),
      pdfImageQuality(75),
      pixivFetchImages(false),
      atlPort(18000),
      proxyPort(3128),
      atlProto(QSsl::SecureProtocols),
      proxyType(QNetworkProxy::NoProxy),
      jsLogConsole(true),
      dontUseNativeFileDialog(false),
      overrideStdFonts(false),
      showTabCloseButtons(true),
      createCoredumps(false),
      overrideUserAgent(false),
      useAdblock(false),
      useScp(false),
      emptyRestore(false),
      debugNetReqLogging(false),
      debugDumpHtml(false),
      globalContextTranslate(false),
      proxyUse(false),
      proxyUseTranslator(false),
      ignoreSSLErrors(false),
      pdfExtractImages(true),
      dlg(nullptr),
      settingsDlgWidth(-1),
      settingsDlgHeight(-1),
      restoreLoadChecked(false)

{
    fontFixed=QStringLiteral("Courier New");
    fontSerif=QStringLiteral("Times New Roman");
    fontSansSerif=QStringLiteral("Verdana");

    savedAuxDir=QDir::homePath();
    savedAuxSaveDir=savedAuxDir;
    overrideFont=QApplication::font();
    fontStandard=QApplication::font().family();

#if WITH_RECOLL
    searchEngine = SE_RECOLL;
#elif WITH_BALOO5
    searchEngine = SE_BALOO5;
#else
    searchEngine = SE_NONE;
#endif

    settingsSaveTimer.setInterval(60000);
    connect(&settingsSaveTimer, &QTimer::timeout, this, &CSettings::writeSettings);
    settingsSaveTimer.start();
}

void CSettings::writeSettings()
{
    if (!gSet) return;
    if (!settingsSaveMutex.tryLock(1000)) return;
    QSettings settings(QStringLiteral("kernel1024"), QStringLiteral("jpreader"));
    QSettings bigdata(QStringLiteral("kernel1024"), QStringLiteral("jpreader-bigdata"));
    settings.beginGroup(QStringLiteral("MainWindow"));
    bigdata.beginGroup(QStringLiteral("main"));
    settings.remove(QString());
    bigdata.remove(QString());

    bigdata.setValue(QStringLiteral("searchHistory"),QVariant::fromValue(gSet->searchHistory));
    bigdata.setValue(QStringLiteral("history"),QVariant::fromValue(gSet->mainHistory));
    bigdata.setValue(QStringLiteral("adblock"),QVariant::fromValue(gSet->adblock));
    bigdata.setValue(QStringLiteral("atlHostHistory"),QVariant::fromValue(atlHostHistory));
    bigdata.setValue(QStringLiteral("scpHostHistory"),QVariant::fromValue(scpHostHistory));
    bigdata.setValue(QStringLiteral("userAgentHistory"),QVariant::fromValue(userAgentHistory));
    bigdata.setValue(QStringLiteral("dictPaths"),QVariant::fromValue(dictPaths));
    bigdata.setValue(QStringLiteral("ctxSearchEngines"),QVariant::fromValue(gSet->ctxSearchEngines));
    bigdata.setValue(QStringLiteral("atlasCertificates"),QVariant::fromValue(gSet->atlCerts));
    bigdata.setValue(QStringLiteral("recentFiles"),QVariant::fromValue(gSet->recentFiles));
    bigdata.setValue(QStringLiteral("userScripts"),QVariant::fromValue(gSet->getUserScripts()));
    bigdata.setValue(QStringLiteral("bookmarks2"),QVariant::fromValue(gSet->bookmarksManager->save()));
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
    settings.setValue(QStringLiteral("javascript"),gSet->webProfile->settings()->
                      testAttribute(QWebEngineSettings::JavascriptEnabled));
    settings.setValue(QStringLiteral("autoloadimages"),gSet->webProfile->settings()->
                      testAttribute(QWebEngineSettings::AutoLoadImages));
    settings.setValue(QStringLiteral("enablePlugins"),gSet->webProfile->settings()->
                      testAttribute(QWebEngineSettings::PluginsEnabled));
    settings.setValue(QStringLiteral("recycledCount"),maxRecycled);

    settings.setValue(QStringLiteral("useAdblock"),useAdblock);
    settings.setValue(QStringLiteral("useOverrideFont"),gSet->ui.useOverrideFont());
    settings.setValue(QStringLiteral("overrideFont"),overrideFont.family());
    settings.setValue(QStringLiteral("overrideFontSize"),overrideFont.pointSize());
    settings.setValue(QStringLiteral("overrideStdFonts"),overrideStdFonts);
    settings.setValue(QStringLiteral("standardFont"),fontStandard);
    settings.setValue(QStringLiteral("fixedFont"),fontFixed);
    settings.setValue(QStringLiteral("serifFont"),fontSerif);
    settings.setValue(QStringLiteral("sansSerifFont"),fontSansSerif);
    settings.setValue(QStringLiteral("forceFontColor"),gSet->ui.forceFontColor());
    settings.setValue(QStringLiteral("forcedFontColor"),forcedFontColor.name());
    settings.setValue(QStringLiteral("gctxHotkey"),gSet->ui.gctxTranHotkey.shortcut().toString());
    settings.setValue(QStringLiteral("translateSubSentences"),gSet->ui.translateSubSentences());

    settings.setValue(QStringLiteral("searchEngine"),searchEngine);
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
    settings.setValue(QStringLiteral("showFavicons"),gSet->webProfile->settings()->
                      testAttribute(QWebEngineSettings::AutoLoadIconsForPage));
    settings.setValue(QStringLiteral("diskCacheSize"),gSet->webProfile->httpCacheMaximumSize());
    settings.setValue(QStringLiteral("jsLogConsole"),jsLogConsole);
    settings.setValue(QStringLiteral("dontUseNativeFileDialog"),dontUseNativeFileDialog);
    settings.setValue(QStringLiteral("defaultSearchEngine"),defaultSearchEngine);
    settings.setValue(QStringLiteral("settingsDlgWidth"),settingsDlgWidth);
    settings.setValue(QStringLiteral("settingsDlgHeight"),settingsDlgHeight);

    settings.setValue(QStringLiteral("pdfExtractImages"),pdfExtractImages);
    settings.setValue(QStringLiteral("pdfImageMaxSize"),pdfImageMaxSize);
    settings.setValue(QStringLiteral("pdfImageQuality"),pdfImageQuality);

    settings.setValue(QStringLiteral("pixivFetchImages"),pixivFetchImages);

    settings.endGroup();
    bigdata.endGroup();
    settingsSaveMutex.unlock();
}

void CSettings::readSettings(QObject *control)
{
    auto g = qobject_cast<CGlobalControl *>(control);
    if (!g) return;

    QSettings settings(QStringLiteral("kernel1024"), QStringLiteral("jpreader"));
    QSettings bigdata(QStringLiteral("kernel1024"), QStringLiteral("jpreader-bigdata"));
    settings.beginGroup(QStringLiteral("MainWindow"));
    bigdata.beginGroup(QStringLiteral("main"));

    g->searchHistory = bigdata.value(QStringLiteral("searchHistory"),QStringList()).toStringList();

    emit g->updateAllQueryLists();

    atlHostHistory = bigdata.value(QStringLiteral("atlHostHistory"),QStringList()).toStringList();
    scpHostHistory = bigdata.value(QStringLiteral("scpHostHistory"),QStringList()).toStringList();
    g->mainHistory = bigdata.value(QStringLiteral("history")).value<CUrlHolderVector>();
    userAgentHistory = bigdata.value(QStringLiteral("userAgentHistory"),QStringList()).toStringList();
    dictPaths = bigdata.value(QStringLiteral("dictPaths"),QStringList()).toStringList();
    g->ctxSearchEngines = bigdata.value(QStringLiteral("ctxSearchEngines")).value<CStringHash>();
    g->atlCerts = bigdata.value(QStringLiteral("atlasCertificates")).value<CSslCertificateHash>();
    g->recentFiles = bigdata.value(QStringLiteral("recentFiles"),QStringList()).toStringList();
    g->initUserScripts(bigdata.value(QStringLiteral("userScripts")).value<CStringHash>());
    g->bookmarksManager->load(bigdata.value(QStringLiteral("bookmarks2"),QByteArray()).toByteArray());
    translatorPairs = bigdata.value(QStringLiteral("translatorPairs")).value<CLangPairVector>();

    int idx=0;
    while (idx<g->mainHistory.count()) {
        if (g->mainHistory.at(idx).url.toString().startsWith(QStringLiteral("data:"),
                                                             Qt::CaseInsensitive))
            g->mainHistory.removeAt(idx);
        else
            idx++;
    }

    g->adblock = bigdata.value(QStringLiteral("adblock")).value<CAdBlockVector>();

    hostingDir = settings.value(QStringLiteral("hostingDir"),QString()).toString();
    hostingUrl = settings.value(QStringLiteral("hostingUrl"),"about:blank").toString();
    maxSearchLimit = settings.value(QStringLiteral("maxLimit"),1000).toInt();
    maxHistory = settings.value(QStringLiteral("maxHistory"),1000).toInt();
    maxRecent = settings.value(QStringLiteral("maxRecent"),10).toInt();
    maxAdblockWhiteList = settings.value(QStringLiteral("maxAdblockWhiteList"),5000).toInt();
    sysBrowser = settings.value(QStringLiteral("browser"),"konqueror").toString();
    sysEditor = settings.value(QStringLiteral("editor"),"kwrite").toString();
    translatorEngine = settings.value(QStringLiteral("tr_engine"),TE_ATLAS).toInt();
    useScp = settings.value(QStringLiteral("scp"),false).toBool();
    scpHost = settings.value(QStringLiteral("scphost"),QString()).toString();
    scpParams = settings.value(QStringLiteral("scpparams"),QString()).toString();
    atlHost = settings.value(QStringLiteral("atlasHost"),"localhost").toString();
    atlPort = static_cast<quint16>(settings.value(QStringLiteral("atlasPort"),18000).toUInt());
    atlToken = settings.value(QStringLiteral("atlasToken"),QString()).toString();
    atlProto = static_cast<QSsl::SslProtocol>(settings.value(QStringLiteral("atlasProto"),
                                                             QSsl::SecureProtocols).toInt());
    savedAuxDir = settings.value(QStringLiteral("auxDir"),QDir::homePath()).toString();
    savedAuxSaveDir = settings.value(QStringLiteral("auxSaveDir"),QDir::homePath()).toString();
    emptyRestore = settings.value(QStringLiteral("emptyRestore"),false).toBool();
    maxRecycled = settings.value(QStringLiteral("recycledCount"),20).toInt();
    bool jsstate = settings.value(QStringLiteral("javascript"),true).toBool();
    g->webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,jsstate);
    g->ui.actionJSUsage->setChecked(jsstate);
    g->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadImages,
                                         settings.value(QStringLiteral("autoloadimages"),true).toBool());
    g->webProfile->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,
                                         settings.value(QStringLiteral("enablePlugins"),false).toBool());

    useAdblock=settings.value(QStringLiteral("useAdblock"),false).toBool();
    g->ui.actionOverrideFont->setChecked(settings.value(QStringLiteral("useOverrideFont"),
                                                        false).toBool());
    overrideFont.setFamily(settings.value(QStringLiteral("overrideFont"),"Verdana").toString());
    overrideFont.setPointSize(settings.value(QStringLiteral("overrideFontSize"),12).toInt());
    overrideStdFonts=settings.value(QStringLiteral("overrideStdFonts"),false).toBool();
    fontStandard=settings.value(QStringLiteral("standardFont"),QApplication::font().family()).toString();
    fontFixed=settings.value(QStringLiteral("fixedFont"),"Courier New").toString();
    fontSerif=settings.value(QStringLiteral("serifFont"),"Times New Roman").toString();
    fontSansSerif=settings.value(QStringLiteral("sansSerifFont"),"Verdana").toString();
    g->ui.actionOverrideFontColor->setChecked(settings.value(
                                                  QStringLiteral("forceFontColor"),false).toBool());
    forcedFontColor=QColor(settings.value(
                               QStringLiteral("forcedFontColor"),"#000000").toString());
    QString hk = settings.value(QStringLiteral("gctxHotkey"),QString()).toString();
    g->ui.actionTranslateSubSentences->setChecked(settings.value(
                                                      QStringLiteral("translateSubSentences"),
                                                      false).toBool());
    if (!hk.isEmpty()) {
        g->ui.gctxTranHotkey.setShortcut(QKeySequence::fromString(hk));
        if (!g->ui.gctxTranHotkey.shortcut().isEmpty())
            g->ui.gctxTranHotkey.setEnabled();
    }
    searchEngine = settings.value(QStringLiteral("searchEngine"),SE_NONE).toInt();
    atlTcpRetryCount = settings.value(QStringLiteral("atlTcpRetryCount"),3).toInt();
    atlTcpTimeout = settings.value(QStringLiteral("atlTcpTimeout"),2).toInt();
    showTabCloseButtons = settings.value(QStringLiteral("showTabCloseButtons"),true).toBool();
    proxyHost = settings.value(QStringLiteral("proxyHost"),QString()).toString();
    proxyPort = static_cast<quint16>(settings.value(QStringLiteral("proxyPort"),3128).toUInt());
    proxyType = static_cast<QNetworkProxy::ProxyType>(
                    settings.value(QStringLiteral("proxyType"),
                                   QNetworkProxy::HttpCachingProxy).toInt());
    proxyLogin = settings.value(QStringLiteral("proxyLogin"),QString()).toString();
    proxyPassword = settings.value(QStringLiteral("proxyPassword"),QString()).toString();
    proxyUse = settings.value(QStringLiteral("proxyUse"),false).toBool();
    proxyUseTranslator = settings.value(QStringLiteral("proxyUseTranslator"),false).toBool();
    bingKey = settings.value(QStringLiteral("bingKey"),QString()).toString();
    yandexKey = settings.value(QStringLiteral("yandexKey"),QString()).toString();
    jsLogConsole = settings.value(QStringLiteral("jsLogConsole"),true).toBool();
    dontUseNativeFileDialog = settings.value(QStringLiteral("dontUseNativeFileDialog"),false).toBool();
    createCoredumps = settings.value(QStringLiteral("createCoredumps"),false).toBool();
    ignoreSSLErrors = settings.value(QStringLiteral("ignoreSSLErrors"),false).toBool();
    g->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,
                                            settings.value(QStringLiteral("showFavicons"),true).toBool());
    defaultSearchEngine = settings.value(QStringLiteral("defaultSearchEngine"),QString()).toString();
    g->webProfile->setHttpCacheMaximumSize(settings.value(QStringLiteral("diskCacheSize"),0).toInt());
    settingsDlgWidth=settings.value(QStringLiteral("settingsDlgWidth"),850).toInt();
    settingsDlgHeight=settings.value(QStringLiteral("settingsDlgHeight"),380).toInt();

    pdfExtractImages = settings.value(QStringLiteral("pdfExtractImages"),true).toBool();
    pdfImageMaxSize = settings.value(QStringLiteral("pdfImageMaxSize"),250).toInt();
    pdfImageQuality = settings.value(QStringLiteral("pdfImageQuality"),75).toInt();

    pixivFetchImages = settings.value(QStringLiteral("pixivFetchImages"),true).toBool();

    overrideUserAgent=settings.value(QStringLiteral("overrideUserAgent"),false).toBool();
    userAgent=settings.value(QStringLiteral("userAgent"),QString()).toString();
    if (userAgent.isEmpty())
        overrideUserAgent=false;
    else if (userAgentHistory.isEmpty())
        userAgentHistory << userAgent;

    if (overrideUserAgent)
        g->webProfile->setHttpUserAgent(userAgent);


    settings.endGroup();
    bigdata.endGroup();
    if (!hostingDir.endsWith('/')) hostingDir.append('/');
    if (!hostingUrl.endsWith('/')) hostingUrl.append('/');

    emit g->updateAllBookmarks();
    g->updateProxyWithMenuUpdate(proxyUse,true);
    g->ui.rebuildLanguageActions(g);
}

void CSettings::settingsDlg()
{
    if (!gSet) return;
    if (dlg) {
        dlg->activateWindow();
        return;
    }
    dlg = new CSettingsDlg();
    dlg->hostingDir->setText(hostingDir);
    dlg->hostingUrl->setText(hostingUrl);
    dlg->maxLimit->setValue(maxSearchLimit);
    dlg->maxHistory->setValue(maxHistory);
    dlg->maxRecent->setValue(maxRecent);
    dlg->editor->setText(sysEditor);
    dlg->browser->setText(sysBrowser);
    dlg->maxRecycled->setValue(maxRecycled);
    dlg->adblockMaxWhiteListItems->setValue(maxAdblockWhiteList);
    dlg->useJS->setChecked(gSet->webProfile->settings()->
                           testAttribute(QWebEngineSettings::JavascriptEnabled));
    dlg->autoloadImages->setChecked(gSet->webProfile->settings()->
                                    testAttribute(QWebEngineSettings::AutoLoadImages));
    dlg->enablePlugins->setChecked(gSet->webProfile->settings()->
                                   testAttribute(QWebEngineSettings::PluginsEnabled));

    dlg->setMainHistory(gSet->mainHistory);
    dlg->setQueryHistory(gSet->searchHistory);
    dlg->setAdblock(gSet->adblock);
    dlg->setUserScripts(gSet->getUserScripts());

    switch (translatorEngine) {
        case TE_GOOGLE: dlg->rbGoogle->setChecked(true); break;
        case TE_ATLAS: dlg->rbAtlas->setChecked(true); break;
        case TE_BINGAPI: dlg->rbBingAPI->setChecked(true); break;
        case TE_YANDEX: dlg->rbYandexAPI->setChecked(true); break;
        case TE_GOOGLE_GTX: dlg->rbGoogleGTX->setChecked(true); break;
        default: dlg->rbAtlas->setChecked(true); break;
    }
    dlg->scpHost->clear();
    if (!scpHostHistory.contains(scpHost))
        scpHostHistory.append(scpHost);
    dlg->scpHost->addItems(scpHostHistory);
    dlg->scpHost->setEditText(scpHost);
    dlg->scpParams->setText(scpParams);
    dlg->cbSCP->setChecked(useScp);
    dlg->atlHost->clear();
    if (!atlHostHistory.contains(atlHost))
        atlHostHistory.append(atlHost);
    dlg->atlHost->addItems(atlHostHistory);
    dlg->atlHost->setEditText(atlHost);
    dlg->atlPort->setValue(atlPort);
    dlg->atlRetryCount->setValue(atlTcpRetryCount);
    dlg->atlRetryTimeout->setValue(atlTcpTimeout);
    dlg->atlToken->setText(atlToken);
    int idx = dlg->atlSSLProto->findData(atlProto);
    if (idx<0 || idx>=dlg->atlSSLProto->count())
        idx = 0;
    dlg->atlSSLProto->setCurrentIndex(idx);
    dlg->updateAtlCertLabel();
    dlg->bingKey->setText(bingKey);
    dlg->yandexKey->setText(yandexKey);
    dlg->emptyRestore->setChecked(emptyRestore);
    dlg->jsLogConsole->setChecked(jsLogConsole);

    dlg->useAd->setChecked(useAdblock);
    dlg->useOverrideFont->setChecked(gSet->ui.useOverrideFont());
    dlg->overrideStdFonts->setChecked(overrideStdFonts);
    dlg->fontOverride->setCurrentFont(overrideFont);
    QFont f = QApplication::font();
    f.setFamily(fontStandard);
    dlg->fontStandard->setCurrentFont(f);
    f.setFamily(fontFixed);
    dlg->fontFixed->setCurrentFont(f);
    f.setFamily(fontSerif);
    dlg->fontSerif->setCurrentFont(f);
    f.setFamily(fontSansSerif);
    dlg->fontSansSerif->setCurrentFont(f);
    dlg->fontOverrideSize->setValue(overrideFont.pointSize());
    dlg->fontOverride->setEnabled(gSet->ui.useOverrideFont());
    dlg->fontOverrideSize->setEnabled(gSet->ui.useOverrideFont());
    dlg->fontStandard->setEnabled(overrideStdFonts);
    dlg->fontFixed->setEnabled(overrideStdFonts);
    dlg->fontSerif->setEnabled(overrideStdFonts);
    dlg->fontSansSerif->setEnabled(overrideStdFonts);
    dlg->overrideFontColor->setChecked(gSet->ui.forceFontColor());
    dlg->updateFontColorPreview(forcedFontColor);
    dlg->gctxHotkey->setKeySequence(gSet->ui.gctxTranHotkey.shortcut());
    dlg->createCoredumps->setChecked(createCoredumps);
#ifndef WITH_RECOLL
    dlg->searchRecoll->setEnabled(false);
#endif
#ifndef WITH_BALOO5
    dlg->searchBaloo5->setEnabled(false);
#endif
    if ((searchEngine==SE_RECOLL) && (dlg->searchRecoll->isEnabled()))
        dlg->searchRecoll->setChecked(true);
    else if ((searchEngine==SE_BALOO5) && (dlg->searchBaloo5->isEnabled()))
        dlg->searchBaloo5->setChecked(true);
    else
        dlg->searchNone->setChecked(true);

    dlg->visualShowTabCloseButtons->setChecked(showTabCloseButtons);

    dlg->debugLogNetReq->setChecked(gSet->ui.actionLogNetRequests->isChecked());
    dlg->debugDumpHtml->setChecked(debugDumpHtml);

    dlg->overrideUserAgent->setChecked(overrideUserAgent);
    dlg->userAgent->setEnabled(overrideUserAgent);
    dlg->userAgent->clear();
    dlg->userAgent->addItems(userAgentHistory);
    dlg->userAgent->lineEdit()->setText(userAgent);

    dlg->dictPaths->clear();
    dlg->dictPaths->addItems(dictPaths);
    dlg->loadedDicts.clear();
    dlg->loadedDicts.append(gSet->dictManager->getLoadedDictionaries());

    dlg->dontUseNativeFileDialogs->setChecked(dontUseNativeFileDialog);
    dlg->cacheSize->setValue(gSet->webProfile->httpCacheMaximumSize()/(1024*1024));
    dlg->ignoreSSLErrors->setChecked(ignoreSSLErrors);
    dlg->visualShowFavicons->setChecked(gSet->webProfile->settings()->
                                        testAttribute(QWebEngineSettings::AutoLoadIconsForPage));

    dlg->setSearchEngines(gSet->ctxSearchEngines);

    dlg->pdfExtractImages->setChecked(pdfExtractImages);
    dlg->pdfImageQuality->setValue(pdfImageQuality);
    dlg->pdfImageMaxSize->setValue(pdfImageMaxSize);

    dlg->pixivNovelExtractor->setChecked(pixivFetchImages);

    // flip proxy use check, for updating controls enabling logic
    dlg->proxyUse->setChecked(true);
    dlg->proxyUse->setChecked(false);

    dlg->proxyHost->setText(proxyHost);
    dlg->proxyPort->setValue(proxyPort);
    dlg->proxyLogin->setText(proxyLogin);
    dlg->proxyPassword->setText(proxyPassword);
    dlg->proxyUse->setChecked(proxyUse);
    dlg->proxyUseTranslator->setChecked(proxyUseTranslator);
    switch (proxyType) {
        case QNetworkProxy::HttpCachingProxy: dlg->proxyType->setCurrentIndex(0); break;
        case QNetworkProxy::HttpProxy: dlg->proxyType->setCurrentIndex(1); break;
        case QNetworkProxy::Socks5Proxy: dlg->proxyType->setCurrentIndex(2); break;
        case QNetworkProxy::NoProxy:
        case QNetworkProxy::DefaultProxy:
        case QNetworkProxy::FtpCachingProxy:
            dlg->proxyType->setCurrentIndex(0);
            break;
    }

    dlg->getCookiesFromStore();

    dlg->resize(settingsDlgWidth,settingsDlgHeight);

    if (dlg->exec()==QDialog::Accepted) {
        hostingDir=dlg->hostingDir->text();
        hostingUrl=dlg->hostingUrl->text();
        maxSearchLimit=dlg->maxLimit->value();
        maxHistory=dlg->maxHistory->value();
        sysEditor=dlg->editor->text();
        sysBrowser=dlg->browser->text();
        maxRecycled=dlg->maxRecycled->value();
        maxRecent=dlg->maxRecent->value();
        maxAdblockWhiteList=dlg->adblockMaxWhiteListItems->value();

        gSet->searchHistory.clear();
        gSet->searchHistory.append(dlg->getQueryHistory());
        emit gSet->updateAllQueryLists();
        gSet->adblock.clear();
        gSet->adblock.append(dlg->getAdblock());
        gSet->adblockWhiteListMutex.lock();
        gSet->adblockWhiteList.clear();
        gSet->adblockWhiteListMutex.unlock();
        gSet->initUserScripts(dlg->getUserScripts());

        translatorPairs = dlg->getLangPairList();
        gSet->ui.rebuildLanguageActions();

        if (!hostingDir.endsWith('/')) hostingDir.append('/');
        if (!hostingUrl.endsWith('/')) hostingUrl.append('/');

        gSet->webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,
                                                   dlg->useJS->isChecked());
        gSet->ui.actionJSUsage->setChecked(dlg->useJS->isChecked());
        gSet->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadImages,
                                                   dlg->autoloadImages->isChecked());
        gSet->webProfile->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,
                                                   dlg->enablePlugins->isChecked());

        if (dlg->rbGoogle->isChecked()) translatorEngine=TE_GOOGLE;
        else if (dlg->rbAtlas->isChecked()) translatorEngine=TE_ATLAS;
        else if (dlg->rbBingAPI->isChecked()) translatorEngine=TE_BINGAPI;
        else if (dlg->rbYandexAPI->isChecked()) translatorEngine=TE_YANDEX;
        else if (dlg->rbGoogleGTX->isChecked()) translatorEngine=TE_GOOGLE_GTX;
        else translatorEngine=TE_ATLAS;
        useScp=dlg->cbSCP->isChecked();
        scpHost=dlg->scpHost->lineEdit()->text();
        scpParams=dlg->scpParams->text();
        atlHost=dlg->atlHost->lineEdit()->text();
        atlPort=static_cast<quint16>(dlg->atlPort->value());
        atlTcpRetryCount=dlg->atlRetryCount->value();
        atlTcpTimeout=dlg->atlRetryTimeout->value();
        atlToken=dlg->atlToken->text();
        atlProto=static_cast<QSsl::SslProtocol>(dlg->atlSSLProto->currentData().toInt());
        bingKey=dlg->bingKey->text();
        yandexKey=dlg->yandexKey->text();
        emptyRestore=dlg->emptyRestore->isChecked();
        jsLogConsole=dlg->jsLogConsole->isChecked();
        if (scpHostHistory.contains(scpHost))
            scpHostHistory.move(scpHostHistory.indexOf(scpHost),0);
        else
            scpHostHistory.prepend(scpHost);
        if (atlHostHistory.contains(atlHost))
            atlHostHistory.move(atlHostHistory.indexOf(atlHost),0);
        else
            atlHostHistory.prepend(atlHost);
        gSet->ui.actionOverrideFont->setChecked(dlg->useOverrideFont->isChecked());
        overrideStdFonts=dlg->overrideStdFonts->isChecked();
        overrideFont=dlg->fontOverride->currentFont();
        overrideFont.setPointSize(dlg->fontOverrideSize->value());
        fontStandard=dlg->fontStandard->currentFont().family();
        fontFixed=dlg->fontFixed->currentFont().family();
        fontSerif=dlg->fontSerif->currentFont().family();
        fontSansSerif=dlg->fontSansSerif->currentFont().family();
        gSet->ui.actionOverrideFontColor->setChecked(dlg->overrideFontColor->isChecked());
        forcedFontColor=dlg->getOverridedFontColor();

        useAdblock=dlg->useAd->isChecked();

        gSet->ui.gctxTranHotkey.setShortcut(dlg->gctxHotkey->keySequence());
        if (!gSet->ui.gctxTranHotkey.shortcut().isEmpty())
            gSet->ui.gctxTranHotkey.setEnabled();
        createCoredumps=dlg->createCoredumps->isChecked();
        if (dlg->searchRecoll->isChecked())
            searchEngine = SE_RECOLL;
        else if (dlg->searchBaloo5->isChecked())
            searchEngine = SE_BALOO5;
        else
            searchEngine = SE_NONE;

        showTabCloseButtons = dlg->visualShowTabCloseButtons->isChecked();

        gSet->ui.actionLogNetRequests->setChecked(dlg->debugLogNetReq->isChecked());
        debugDumpHtml = dlg->debugDumpHtml->isChecked();

        overrideUserAgent = dlg->overrideUserAgent->isChecked();
        if (overrideUserAgent) {
            userAgent = dlg->userAgent->lineEdit()->text();
            if (!userAgentHistory.contains(userAgent))
                userAgentHistory << userAgent;

            if (userAgent.isEmpty())
                overrideUserAgent=false;
        }

        if (overrideUserAgent)
            gSet->webProfile->setHttpUserAgent(userAgent);

        QStringList sl;
        sl.reserve(dlg->dictPaths->count());
        for (int i=0;i<dlg->dictPaths->count();i++)
            sl.append(dlg->dictPaths->item(i)->text());
        if (compareStringLists(dictPaths,sl)!=0) {
            dictPaths.clear();
            dictPaths.append(sl);
            gSet->dictManager->loadDictionaries(dictPaths, dictIndexDir);
        }

        gSet->ctxSearchEngines = dlg->getSearchEngines();

        pdfExtractImages = dlg->pdfExtractImages->isChecked();
        pdfImageMaxSize = dlg->pdfImageMaxSize->value();
        pdfImageQuality = dlg->pdfImageQuality->value();

        pixivFetchImages = dlg->pixivNovelExtractor->isChecked();

        dontUseNativeFileDialog = dlg->dontUseNativeFileDialogs->isChecked();
        gSet->webProfile->setHttpCacheMaximumSize(dlg->cacheSize->value()*1024*1024);
        ignoreSSLErrors = dlg->ignoreSSLErrors->isChecked();
        gSet->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,
                                                   dlg->visualShowFavicons->isChecked());
        proxyHost = dlg->proxyHost->text();
        proxyPort = static_cast<quint16>(dlg->proxyPort->value());
        proxyLogin = dlg->proxyLogin->text();
        proxyPassword = dlg->proxyPassword->text();
        proxyUse = dlg->proxyUse->isChecked();
        proxyUseTranslator = dlg->proxyUseTranslator->isChecked();
        switch (dlg->proxyType->currentIndex()) {
            case 0: proxyType = QNetworkProxy::HttpCachingProxy; break;
            case 1: proxyType = QNetworkProxy::HttpProxy; break;
            case 2: proxyType = QNetworkProxy::Socks5Proxy; break;
            default: proxyType = QNetworkProxy::HttpCachingProxy; break;
        }

        gSet->updateProxyWithMenuUpdate(proxyUse,true);
        emit settingsUpdated();

        settingsDlgWidth=dlg->width();
        settingsDlgHeight=dlg->height();
    }
    connect(dlg,&CSettingsDlg::destroyed,this,[this](){
        dlg=nullptr;
    });
    dlg->deleteLater();
}

void CSettings::setTranslationEngine(int engine)
{
    translatorEngine = engine;
    emit settingsUpdated();
}

void CSettings::checkRestoreLoad(CMainWindow *w)
{
    if (restoreLoadChecked) return;
    restoreLoadChecked = true;

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

    for (int i=0;i<gSet->mainWindows.count();i++) {
        for (int j=0;j<gSet->mainWindows.at(i)->tabMain->count();j++) {
            auto sn = qobject_cast<CSnippetViewer *>(gSet->mainWindows.at(i)->tabMain->widget(j));
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
