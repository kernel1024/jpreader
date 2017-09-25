#include <QSettings>
#include <QDir>
#include <QApplication>
#include <QWebEngineSettings>
#include <QMessageBox>
#include <goldendictlib/goldendictmgr.hh>
#include "settings.h"
#include "globalcontrol.h"
#include "miniqxt/qxtglobalshortcut.h"
#include "genericfuncs.h"
#include "snviewer.h"
#include "userscript.h"

CSettings::CSettings(QObject *parent)
    : QObject(parent)
{
    snippetColors << QColor(Qt::red) << QColor(Qt::green) << QColor(Qt::blue) << QColor(Qt::cyan) <<
                     QColor(Qt::magenta) << QColor(Qt::darkRed) << QColor(Qt::darkGreen) <<
                     QColor(Qt::darkBlue) << QColor(Qt::darkCyan) << QColor(Qt::darkMagenta) <<
                     QColor(Qt::darkYellow) << QColor(Qt::gray);

    maxSearchLimit=1000;
    maxHistory=5000;
    maxRecent=10;
    maxBookmarksCnt=10;
    maxAdblockWhiteList=5000;
    useAdblock=false;
    restoreLoadChecked=false;
    globalContextTranslate=false;
    emptyRestore=false;
    createCoredumps=false;
    ignoreSSLErrors=false;
    jsLogConsole=true;
    dontUseNativeFileDialog=false;
    forcedCharset=QString(); // autodetect
    overrideUserAgent=false;
    overrideStdFonts=false;
    fontFixed=QString("Courier New");
    fontSerif=QString("Times New Roman");
    fontSansSerif=QString("Verdana");
    atlTcpRetryCount=3;
    atlTcpTimeout=2;
    atlToken=QString();
    atlProto=QSsl::SecureProtocols;
    bingID=QString();
    bingKey=QString();
    yandexKey=QString();
    proxyHost=QString();
    proxyPort=3128;
    proxyLogin=QString();
    proxyPassword=QString();
    proxyUse=false;
    proxyUseTranslator=false;
    proxyType=QNetworkProxy::HttpCachingProxy;
    debugNetReqLogging=false;
    debugDumpHtml=false;
    dlg=nullptr;
    settingsDlgWidth=850;
    settingsDlgHeight=380;

    charsetHistory.clear();
    scpHostHistory.clear();
    atlHostHistory.clear();
    dictPaths.clear();
    userAgent.clear();
    userAgentHistory.clear();
    defaultSearchEngine.clear();
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
    if (gSet==nullptr) return;
    if (!settingsSaveMutex.tryLock(1000)) return;
    QSettings settings("kernel1024", "jpreader");
    QSettings bigdata("kernel1024", "jpreader-bigdata");
    settings.beginGroup("MainWindow");
    bigdata.beginGroup("main");
    settings.remove("");
    bigdata.remove("");

    bigdata.setValue("searchHistory",QVariant::fromValue(gSet->searchHistory));
    bigdata.setValue("bookmarks_list",QVariant::fromValue(gSet->bookmarks));
    bigdata.setValue("history",QVariant::fromValue(gSet->mainHistory));
    bigdata.setValue("adblock",QVariant::fromValue(gSet->adblock));
    bigdata.setValue("atlHostHistory",QVariant::fromValue(atlHostHistory));
    bigdata.setValue("scpHostHistory",QVariant::fromValue(scpHostHistory));
    bigdata.setValue("userAgentHistory",QVariant::fromValue(userAgentHistory));
    bigdata.setValue("dictPaths",QVariant::fromValue(dictPaths));
    bigdata.setValue("ctxSearchEngines",QVariant::fromValue(gSet->ctxSearchEngines));
    bigdata.setValue("atlasCertificates",QVariant::fromValue(gSet->atlCerts));
    bigdata.setValue("recentFiles",QVariant::fromValue(gSet->recentFiles));
    bigdata.setValue("userScripts",QVariant::fromValue(gSet->getUserScripts()));

    settings.setValue("hostingDir",hostingDir);
    settings.setValue("hostingUrl",hostingUrl);
    settings.setValue("maxLimit",maxSearchLimit);
    settings.setValue("maxHistory",maxHistory);
    settings.setValue("maxRecent",maxRecent);
    settings.setValue("maxBookmarksCnt",maxBookmarksCnt);
    settings.setValue("maxAdblockWhiteList",maxAdblockWhiteList);
    settings.setValue("browser",sysBrowser);
    settings.setValue("editor",sysEditor);
    settings.setValue("tr_engine",translatorEngine);
    settings.setValue("scp",useScp);
    settings.setValue("scphost",scpHost);
    settings.setValue("scpparams",scpParams);
    settings.setValue("atlasHost",atlHost);
    settings.setValue("atlasPort",atlPort);
    settings.setValue("atlasToken",atlToken);
    settings.setValue("atlasProto",static_cast<int>(atlProto));
    settings.setValue("auxDir",savedAuxDir);
    settings.setValue("auxSaveDir",savedAuxSaveDir);
    settings.setValue("emptyRestore",emptyRestore);
    settings.setValue("javascript",gSet->webProfile->settings()->
                      testAttribute(QWebEngineSettings::JavascriptEnabled));
    settings.setValue("autoloadimages",gSet->webProfile->settings()->
                      testAttribute(QWebEngineSettings::AutoLoadImages));
    settings.setValue("enablePlugins",gSet->webProfile->settings()->
                      testAttribute(QWebEngineSettings::PluginsEnabled));
    settings.setValue("recycledCount",maxRecycled);

    settings.setValue("useAdblock",useAdblock);
    settings.setValue("useOverrideFont",gSet->ui.useOverrideFont());
    settings.setValue("overrideFont",overrideFont.family());
    settings.setValue("overrideFontSize",overrideFont.pointSize());
    settings.setValue("overrideStdFonts",overrideStdFonts);
    settings.setValue("standardFont",fontStandard);
    settings.setValue("fixedFont",fontFixed);
    settings.setValue("serifFont",fontSerif);
    settings.setValue("sansSerifFont",fontSansSerif);
    settings.setValue("forceFontColor",gSet->ui.forceFontColor());
    settings.setValue("forcedFontColor",forcedFontColor.name());
    settings.setValue("gctxHotkey",gSet->ui.gctxTranHotkey.shortcut().toString());
    settings.setValue("translateSubSentences",gSet->ui.translateSubSentences());

    settings.setValue("searchEngine",searchEngine);
    settings.setValue("atlTcpRetryCount",atlTcpRetryCount);
    settings.setValue("atlTcpTimeout",atlTcpTimeout);
    settings.setValue("showTabCloseButtons",showTabCloseButtons);
    settings.setValue("proxyHost",proxyHost);
    settings.setValue("proxyPort",proxyPort);
    settings.setValue("proxyLogin",proxyLogin);
    settings.setValue("proxyPassword",proxyPassword);
    settings.setValue("proxyUse",proxyUse);
    settings.setValue("proxyType",proxyType);
    settings.setValue("proxyUseTranslator",proxyUseTranslator);
    settings.setValue("bingID",bingID);
    settings.setValue("bingKey",bingKey);
    settings.setValue("yandexKey",yandexKey);
    settings.setValue("createCoredumps",createCoredumps);
    settings.setValue("overrideUserAgent",overrideUserAgent);
    settings.setValue("userAgent",userAgent);
    settings.setValue("ignoreSSLErrors",ignoreSSLErrors);
    settings.setValue("showFavicons",gSet->webProfile->settings()->
                      testAttribute(QWebEngineSettings::AutoLoadIconsForPage));
    settings.setValue("diskCacheSize",gSet->webProfile->httpCacheMaximumSize());
    settings.setValue("jsLogConsole",jsLogConsole);
    settings.setValue("dontUseNativeFileDialog",dontUseNativeFileDialog);
    settings.setValue("defaultSearchEngine",defaultSearchEngine);
    settings.setValue("settingsDlgWidth",settingsDlgWidth);
    settings.setValue("settingsDlgHeight",settingsDlgHeight);
    settings.endGroup();
    bigdata.endGroup();
    settingsSaveMutex.unlock();
}

void CSettings::readSettings(QObject *control)
{
    CGlobalControl* g = qobject_cast<CGlobalControl *>(control);
    if (g==nullptr) return;

    QSettings settings("kernel1024", "jpreader");
    QSettings bigdata("kernel1024", "jpreader-bigdata");
    settings.beginGroup("MainWindow");
    bigdata.beginGroup("main");

    g->searchHistory = bigdata.value("searchHistory",QStringList()).value<QStringList>();

    g->updateAllQueryLists();

    atlHostHistory = bigdata.value("atlHostHistory",QStringList()).value<QStringList>();
    scpHostHistory = bigdata.value("scpHostHistory",QStringList()).value<QStringList>();
    g->mainHistory = bigdata.value("history").value<QUHList>();
    userAgentHistory = bigdata.value("userAgentHistory",QStringList()).value<QStringList>();
    dictPaths = bigdata.value("dictPaths",QStringList()).value<QStringList>();
    g->ctxSearchEngines = bigdata.value("ctxSearchEngines").value<QStrHash>();
    g->atlCerts = bigdata.value("atlasCertificates").value<QSslCertificateHash>();
    g->recentFiles = bigdata.value("recentFiles",QStringList()).value<QStringList>();
    g->initUserScripts(bigdata.value("userScripts").value<QStrHash>());

    int idx=0;
    while (idx<g->mainHistory.count()) {
        if (g->mainHistory.at(idx).url.toString().startsWith("data:",Qt::CaseInsensitive))
            g->mainHistory.removeAt(idx);
        else
            idx++;
    }

    if (bigdata.contains("bookmarks")) {
        g->bookmarks.clear();
        QBookmarksMap bm = bigdata.value("bookmarks").value<QBookmarksMap>();
        foreach (const QString &title, bm.keys())
            g->bookmarks << qMakePair(title, bm.value(title).toString());
    } else
        g->bookmarks = bigdata.value("bookmarks_list").value<QBookmarks>();

    g->adblock = bigdata.value("adblock").value<CAdBlockList>();

    hostingDir = settings.value("hostingDir","").toString();
    hostingUrl = settings.value("hostingUrl","about:blank").toString();
    maxSearchLimit = settings.value("maxLimit",1000).toInt();
    maxHistory = settings.value("maxHistory",1000).toInt();
    maxRecent = settings.value("maxRecent",10).toInt();
    maxBookmarksCnt = settings.value("maxBookmarksCnt",15).toInt();
    maxAdblockWhiteList = settings.value("maxAdblockWhiteList",5000).toInt();
    sysBrowser = settings.value("browser","konqueror").toString();
    sysEditor = settings.value("editor","kwrite").toString();
    translatorEngine = settings.value("tr_engine",TE_ATLAS).toInt();
    useScp = settings.value("scp",false).toBool();
    scpHost = settings.value("scphost","").toString();
    scpParams = settings.value("scpparams","").toString();
    atlHost = settings.value("atlasHost","localhost").toString();
    atlPort = static_cast<quint16>(settings.value("atlasPort",18000).toUInt());
    atlToken = settings.value("atlasToken","").toString();
    atlProto = static_cast<QSsl::SslProtocol>(settings.value("atlasProto",QSsl::SecureProtocols).toInt());
    savedAuxDir = settings.value("auxDir",QDir::homePath()).toString();
    savedAuxSaveDir = settings.value("auxSaveDir",QDir::homePath()).toString();
    emptyRestore = settings.value("emptyRestore",false).toBool();
    maxRecycled = settings.value("recycledCount",20).toInt();
    bool jsstate = settings.value("javascript",true).toBool();
    g->webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,jsstate);
    g->ui.actionJSUsage->setChecked(jsstate);
    g->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadImages,
                                         settings.value("autoloadimages",true).toBool());
    g->webProfile->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,
                                         settings.value("enablePlugins",false).toBool());

    useAdblock=settings.value("useAdblock",false).toBool();
    g->ui.actionOverrideFont->setChecked(settings.value("useOverrideFont",false).toBool());
    overrideFont.setFamily(settings.value("overrideFont","Verdana").toString());
    overrideFont.setPointSize(settings.value("overrideFontSize",12).toInt());
    overrideStdFonts=settings.value("overrideStdFonts",false).toBool();
    fontStandard=settings.value("standardFont",QApplication::font().family()).toString();
    fontFixed=settings.value("fixedFont","Courier New").toString();
    fontSerif=settings.value("serifFont","Times New Roman").toString();
    fontSansSerif=settings.value("sansSerifFont","Verdana").toString();
    g->ui.actionOverrideFontColor->setChecked(settings.value("forceFontColor",false).toBool());
    forcedFontColor=QColor(settings.value("forcedFontColor","#000000").toString());
    QString hk = settings.value("gctxHotkey",QString()).toString();
    g->ui.actionTranslateSubSentences->setChecked(settings.value("translateSubSentences",false).toBool());
    if (!hk.isEmpty()) {
        g->ui.gctxTranHotkey.setShortcut(QKeySequence::fromString(hk));
        if (!g->ui.gctxTranHotkey.shortcut().isEmpty())
            g->ui.gctxTranHotkey.setEnabled();
    }
    searchEngine = settings.value("searchEngine",SE_NONE).toInt();
    atlTcpRetryCount = settings.value("atlTcpRetryCount",3).toInt();
    atlTcpTimeout = settings.value("atlTcpTimeout",2).toInt();
    showTabCloseButtons = settings.value("showTabCloseButtons",true).toBool();
    proxyHost = settings.value("proxyHost",QString()).toString();
    proxyPort = static_cast<quint16>(settings.value("proxyPort",3128).toUInt());
    proxyType = static_cast<QNetworkProxy::ProxyType>(settings.value("proxyType",
                                                                     QNetworkProxy::HttpCachingProxy).toInt());
    proxyLogin = settings.value("proxyLogin",QString()).toString();
    proxyPassword = settings.value("proxyPassword",QString()).toString();
    proxyUse = settings.value("proxyUse",false).toBool();
    proxyUseTranslator = settings.value("proxyUseTranslator",false).toBool();
    bingID = settings.value("bingID",QString()).toString();
    bingKey = settings.value("bingKey",QString()).toString();
    yandexKey = settings.value("yandexKey",QString()).toString();
    jsLogConsole = settings.value("jsLogConsole",true).toBool();
    dontUseNativeFileDialog = settings.value("dontUseNativeFileDialog",false).toBool();
    createCoredumps = settings.value("createCoredumps",false).toBool();
    ignoreSSLErrors = settings.value("ignoreSSLErrors",false).toBool();
    g->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,
                                            settings.value("showFavicons",true).toBool());
    defaultSearchEngine = settings.value("defaultSearchEngine",QString()).toString();
    g->webProfile->setHttpCacheMaximumSize(settings.value("diskCacheSize",0).toInt());
    settingsDlgWidth=settings.value("settingsDlgWidth",850).toInt();
    settingsDlgHeight=settings.value("settingsDlgHeight",380).toInt();

    overrideUserAgent=settings.value("overrideUserAgent",false).toBool();
    userAgent=settings.value("userAgent",QString()).toString();
    if (userAgent.isEmpty())
        overrideUserAgent=false;
    else if (userAgentHistory.isEmpty())
        userAgentHistory << userAgent;

    if (overrideUserAgent)
        g->webProfile->setHttpUserAgent(userAgent);


    settings.endGroup();
    bigdata.endGroup();
    if (hostingDir.right(1)!="/") hostingDir=hostingDir+"/";
    if (hostingUrl.right(1)!="/") hostingUrl=hostingUrl+"/";
    g->updateAllBookmarks();
    g->updateProxyWithMenuUpdate(proxyUse,true);
}

void CSettings::settingsDlg()
{
    if (gSet==nullptr) return;
    if (dlg!=nullptr) {
        dlg->activateWindow();
        return;
    }
    dlg = new CSettingsDlg();
    dlg->hostingDir->setText(hostingDir);
    dlg->hostingUrl->setText(hostingUrl);
    dlg->maxLimit->setValue(maxSearchLimit);
    dlg->maxHistory->setValue(maxHistory);
    dlg->maxRecent->setValue(maxRecent);
    dlg->maxBookmarksCnt->setValue(maxBookmarksCnt);
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

    dlg->setBookmarks(gSet->bookmarks);
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
    dlg->bingID->setText(bingID);
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
        maxBookmarksCnt=dlg->maxBookmarksCnt->value();
        maxAdblockWhiteList=dlg->adblockMaxWhiteListItems->value();

        gSet->searchHistory.clear();
        gSet->searchHistory.append(dlg->getQueryHistory());
        gSet->updateAllQueryLists();
        gSet->bookmarks = dlg->getBookmarks();
        gSet->updateAllBookmarks();
        gSet->adblock.clear();
        gSet->adblock.append(dlg->getAdblock());
        gSet->adblockWhiteListMutex.lock();
        gSet->adblockWhiteList.clear();
        gSet->adblockWhiteListMutex.unlock();
        gSet->initUserScripts(dlg->getUserScripts());

        if (hostingDir.right(1)!="/") hostingDir=hostingDir+"/";
        if (hostingUrl.right(1)!="/") hostingUrl=hostingUrl+"/";

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
        bingID=dlg->bingID->text();
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
        sl.clear();
        for (int i=0;i<dlg->dictPaths->count();i++)
            sl.append(dlg->dictPaths->item(i)->text());
        if (compareStringLists(dictPaths,sl)!=0) {
            dictPaths.clear();
            dictPaths.append(sl);
            gSet->dictManager->loadDictionaries(dictPaths, dictIndexDir);
        }

        gSet->ctxSearchEngines = dlg->getSearchEngines();

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
    connect(dlg,&CSettingsDlg::destroyed,[this](){
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

    QList<QUrl> urls;
    urls.clear();
    QSettings settings("kernel1024", "jpreader-tabs");
    settings.beginGroup("OpenedTabs");
    int cnt = settings.value("tabsCnt", 0).toInt();
    for (int i=0;i<cnt;i++) {
        QUrl u = settings.value(QString("tab_%1").arg(i),QUrl()).toUrl();
        if (u.isValid() && !u.isEmpty())
            urls << u;
    }
    settings.endGroup();

    if (!urls.isEmpty()) {
        if (QMessageBox::question(w,tr("JPReader"),tr("Program crashed in previous run. Restore all tabs?"),
                                  QMessageBox::Yes,QMessageBox::No) == QMessageBox::Yes) {
            for (int i=0;i<cnt;i++)
                new CSnippetViewer(w,urls.at(i));
        }
    }
}

QList<QUrl> CSettings::getTabsList() const
{
    QList<QUrl> res;
    if (gSet==nullptr) return res;

    for (int i=0;i<gSet->mainWindows.count();i++) {
        for (int j=0;j<gSet->mainWindows.at(i)->tabMain->count();j++) {
            CSnippetViewer* sn = qobject_cast<CSnippetViewer *>(gSet->mainWindows.at(i)->tabMain->widget(j));
            if (sn==nullptr) continue;
            QUrl url = sn->getUrl();
            if (url.isValid() && !url.isEmpty())
                res << url;
        }
    }

    return res;
}

void CSettings::writeTabsListPrivate(const QList<QUrl> tabList)
{
    QSettings tabs("kernel1024", "jpreader-tabs");
    tabs.beginGroup("OpenedTabs");
    tabs.remove("");
    tabs.setValue("tabsCnt", tabList.count());
    for (int i=0;i<tabList.count();i++)
        tabs.setValue(QString("tab_%1").arg(i),tabList.at(i));
    tabs.endGroup();
}

void CSettings::clearTabsList()
{
    QList<QUrl> tabs;
    writeTabsListPrivate(tabs);
}

void CSettings::writeTabsList()
{
    writeTabsListPrivate(getTabsList());
}
