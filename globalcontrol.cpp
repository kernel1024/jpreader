#include <QNetworkProxy>
#include <QMessageBox>
#include <QLocalSocket>
#include <QWebEngineSettings>
#include <QNetworkCookieJar>
#include <QStandardPaths>

#ifdef WEBENGINE_56
#include <QWebEngineCookieStoreClient>
#endif

#include "mainwindow.h"
#include "settingsdlg.h"
#include "miniqxt/qxtglobalshortcut.h"
#include "goldendictmgr.h"

#include "globalcontrol.h"
#include "lighttranslator.h"
#include "auxtranslator.h"
#include "translator.h"
#include "genericfuncs.h"
#include "auxtranslator_adaptor.h"
#include "miniqxt/qxttooltip.h"
#include "auxdictionary.h"
#include "downloadmanager.h"

#define IPC_EOF "\n###"

CGlobalControl::CGlobalControl(QApplication *parent) :
    QObject(parent)
{
    ipcServer = NULL;
    if (!setupIPC())
        return;

    snippetColors << QColor(Qt::red) << QColor(Qt::green) << QColor(Qt::blue) << QColor(Qt::cyan) <<
                     QColor(Qt::magenta) << QColor(Qt::darkRed) << QColor(Qt::darkGreen) <<
                     QColor(Qt::darkBlue) << QColor(Qt::darkCyan) << QColor(Qt::darkMagenta) <<
                     QColor(Qt::darkYellow) << QColor(Qt::gray);

    maxLimit=1000;
    maxHistory=5000;
    useAdblock=false;
    globalContextTranslate=false;
    blockTabCloseActive=false;
    adblock.clear();
    cleaningState=false;
    restoreLoadChecked=false;
    emptyRestore=false;
    createCoredumps=false;
    ignoreSSLErrors=false;
    showFavicons=false;
    forcedCharset=""; // autodetect
    createdFiles.clear();
    recycleBin.clear();
    mainHistory.clear();
    searchHistory.clear();
    scpHostHistory.clear();
    atlHostHistory.clear();
    dictPaths.clear();
    overrideUserAgent=false;
    userAgent.clear();
    userAgentHistory.clear();
    favicons.clear();

    appIcon.addFile(":/img/globe16");
    appIcon.addFile(":/img/globe32");
    appIcon.addFile(":/img/globe48");
    appIcon.addFile(":/img/globe128");

    overrideStdFonts=false;
    overrideFont=QApplication::font();
    fontStandard=QApplication::font().family();
    fontFixed="Courier New";
    fontSerif="Times New Roman";
    fontSansSerif="Verdana";

    logWindow = new CLogDisplay();

    downloadManager = new CDownloadManager();

    atlTcpRetryCount = 3;
    atlTcpTimeout = 2;
    bingID = QString();
    bingKey = QString();
    yandexKey = QString();

    proxyHost = QString();
    proxyPort = 3128;
    proxyLogin = QString();
    proxyPassword = QString();
    proxyUse = false;
    proxyUseTranslator = false;
    proxyType = QNetworkProxy::HttpCachingProxy;

#if WITH_RECOLL
    searchEngine = SE_RECOLL;
#elif WITH_BALOO5
    searchEngine = SE_BALOO5;
#else
    searchEngine = SE_NONE;
#endif

    dlg = NULL;
    activeWindow = NULL;
    lightTranslator = NULL;
    auxDictionary = NULL;

    actionGlobalTranslator = new QAction(tr("Global context translator"),this);
    actionGlobalTranslator->setCheckable(true);
    actionGlobalTranslator->setChecked(false);

    actionSelectionDictionary = new QAction(tr("Dictionary search"),this);
    actionSelectionDictionary->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_F9));
    actionSelectionDictionary->setCheckable(true);
    actionSelectionDictionary->setChecked(false);

    actionUseProxy = new QAction(tr("Use proxy"),this);
    actionUseProxy->setCheckable(true);
    actionUseProxy->setChecked(false);

    actionJSUsage = new QAction(tr("Enable JavaScript"),this);
    actionJSUsage->setCheckable(true);
    actionJSUsage->setChecked(false);

    actionSnippetAutotranslate = new QAction(tr("Autotranslate snippet text"),this);
    actionSnippetAutotranslate->setCheckable(true);
    actionSnippetAutotranslate->setChecked(false);

    translationMode = new QActionGroup(this);

    actionTMAdditive = new QAction(tr("Additive"),this);
    actionTMAdditive->setCheckable(true);
    actionTMAdditive->setActionGroup(translationMode);
    actionTMAdditive->setData(TM_ADDITIVE);

    actionTMOverwriting = new QAction(tr("Overwriting"),this);
    actionTMOverwriting->setCheckable(true);
    actionTMOverwriting->setActionGroup(translationMode);
    actionTMOverwriting->setData(TM_OVERWRITING);

    actionTMTooltip = new QAction(tr("Tooltip"),this);
    actionTMTooltip->setCheckable(true);
    actionTMTooltip->setActionGroup(translationMode);
    actionTMTooltip->setData(TM_TOOLTIP);

    actionTMAdditive->setChecked(true);

    sourceLanguage = new QActionGroup(this);

    actionLSJapanese = new QAction(tr("Japanese"),this);
    actionLSJapanese->setCheckable(true);
    actionLSJapanese->setActionGroup(sourceLanguage);
    actionLSJapanese->setData(LS_JAPANESE);

    actionLSChineseSimplified = new QAction(tr("Chinese simplified"),this);
    actionLSChineseSimplified->setCheckable(true);
    actionLSChineseSimplified->setActionGroup(sourceLanguage);
    actionLSChineseSimplified->setData(LS_CHINESESIMP);

    actionLSChineseTraditional = new QAction(tr("Chinese traditional"),this);
    actionLSChineseTraditional->setCheckable(true);
    actionLSChineseTraditional->setActionGroup(sourceLanguage);
    actionLSChineseTraditional->setData(LS_CHINESETRAD);

    actionLSKorean = new QAction(tr("Korean"),this);
    actionLSKorean->setCheckable(true);
    actionLSKorean->setActionGroup(sourceLanguage);
    actionLSKorean->setData(LS_KOREAN);

    actionLSJapanese->setChecked(true);

    actionAutoTranslate = new QAction(QIcon::fromTheme("document-edit-decrypt"),tr("Automatic translation"),this);
    actionAutoTranslate->setCheckable(true);
    actionAutoTranslate->setChecked(false);
    actionAutoTranslate->setShortcut(Qt::Key_F8);

    actionOverrideFont = new QAction(QIcon::fromTheme("character-set"),tr("Override font for translated text"),this);
    actionOverrideFont->setCheckable(true);
    actionOverrideFont->setChecked(false);

    actionAutoloadImages = new QAction(QIcon::fromTheme("view-preview"),tr("Autoload images"),this);
    actionAutoloadImages->setCheckable(true);
    actionAutoloadImages->setChecked(false);

    actionOverrideFontColor = new QAction(QIcon::fromTheme("format-text-color"),tr("Force translated text color"),this);
    actionOverrideFontColor->setCheckable(true);
    actionOverrideFontColor->setChecked(false);

    actionLogNetRequests = new QAction(tr("Log network requests"),this);
    actionLogNetRequests->setCheckable(true);
    actionLogNetRequests->setChecked(false);
    debugNetReqLogging=false;

    auxTranslatorDBus = new CAuxTranslator(this);
    new AuxtranslatorAdaptor(auxTranslatorDBus);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/",auxTranslatorDBus);
    dbus.registerService(DBUS_NAME);

    connect(parent,SIGNAL(focusChanged(QWidget*,QWidget*)),this,SLOT(focusChanged(QWidget*,QWidget*)));
    connect(parent,SIGNAL(aboutToQuit()),this,SLOT(preShutdown()));
    connect(QApplication::clipboard(),SIGNAL(changed(QClipboard::Mode)),
            this,SLOT(clipboardChanged(QClipboard::Mode)));
    connect(actionUseProxy,SIGNAL(toggled(bool)),
            this,SLOT(updateProxy(bool)));
    connect(actionJSUsage,SIGNAL(toggled(bool)),
            this,SLOT(toggleJSUsage(bool)));
    connect(actionLogNetRequests,SIGNAL(toggled(bool)),
            this,SLOT(toggleLogNetRequests(bool)));

    gctxTranHotkey = new QxtGlobalShortcut(this);
    gctxTranHotkey->setDisabled();
    connect(gctxTranHotkey,SIGNAL(activated()),actionGlobalTranslator,SLOT(toggle()));

    readSettings();

    webProfile = new QWebEngineProfile("jpreader",this);

    QString fs = QString();
    fs = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    if (fs.isEmpty()) fs = QDir::homePath() + QDir::separator() + tr(".config");
    if (!fs.endsWith(QDir::separator())) fs += QDir::separator();

    QString fcache = fs + tr("cache") + QDir::separator();
    webProfile->setCachePath(fcache);
    QString fdata = fs + tr("local_storage") + QDir::separator();
    webProfile->setPersistentStoragePath(fdata);

    webProfile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    webProfile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

    connect(webProfile, SIGNAL(downloadRequested(QWebEngineDownloadItem*)),
            downloadManager, SLOT(handleDownload(QWebEngineDownloadItem*)));

#ifdef WEBENGINE_56
    webProfile->setRequestInterceptor(new CSpecUrlInterceptor());
    webProfile->setCookieStoreClient(new QWebEngineCookieStoreClient());
    webProfile->installUrlSchemeHandler(new CSpecGDSchemeHandler(QByteArray("gdlookup")));

    connect(webProfile->cookieStoreClient(), SIGNAL(cookieAdded(QNetworkCookie)),
            this, SLOT(cookieAdded(QNetworkCookie)));
    connect(webProfile->cookieStoreClient(), SIGNAL(cookieRemoved(QNetworkCookie)),
            this, SLOT(cookieRemoved(QNetworkCookie)));
#endif

    dictIndexDir = fs + tr("dictIndex") + QDir::separator();
    QDir dictIndex(dictIndexDir);
    if (!dictIndex.exists())
        if (!dictIndex.mkpath(dictIndexDir)) {
            qCritical() << "Unable to create directory for dictionary indexes: " << dictIndexDir;
            dictIndexDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        }

    dictManager = new CGoldenDictMgr(this);
    dictNetMan = new ArticleNetworkAccessManager(this,dictManager);

    auxNetManager = new QNetworkAccessManager(this);

    settingsSaveTimer.setInterval(60000);
    connect(&settingsSaveTimer,SIGNAL(timeout()),this,SLOT(writeSettings()));
    settingsSaveTimer.start();

    tabsListTimer.setInterval(30000);
    connect(&tabsListTimer,SIGNAL(timeout()),this,SLOT(writeTabsList()));
    tabsListTimer.start();

    QTimer::singleShot(1500,dictManager,SLOT(loadDictionaries()));
}

bool CGlobalControl::setupIPC()
{
    QString serverName = IPC_NAME;
    serverName.replace(QRegExp("[^\\w\\-. ]"), "");

    QLocalSocket *socket = new QLocalSocket();
    socket->connectToServer(serverName);
    if (socket->waitForConnected(1000)){
        // Connected. Process is already running, send message to it
        sendIPCMessage(socket,"newWindow");
        socket->flush();
        socket->close();
        socket->deleteLater();
        return false;
    } else {
        // New process. Startup server and gSet normally, listen for new instances
        ipcServer = new QLocalServer();
        ipcServer->removeServer(serverName);
        ipcServer->listen(serverName);
        connect(ipcServer, SIGNAL(newConnection()), this, SLOT(ipcMessageReceived()));
    }
    return true;
}

void  CGlobalControl::sendIPCMessage(QLocalSocket *socket, const QString &msg)
{
    if (socket==NULL) return;

    QString s = QString("%1%2").arg(msg).arg(IPC_EOF);
    socket->write(s.toUtf8());
}

bool CGlobalControl::useOverrideFont()
{
    return actionOverrideFont->isChecked();
}

bool CGlobalControl::autoTranslate()
{
    return actionAutoTranslate->isChecked();
}

bool CGlobalControl::forceFontColor()
{
    return actionOverrideFontColor->isChecked();
}

void CGlobalControl::writeSettings()
{
    if (!settingsSaveMutex.tryLock(1000)) return;
    QSettings settings("kernel1024", "jpreader");
    QSettings bigdata("kernel1024", "jpreader-bigdata");
    settings.beginGroup("MainWindow");
    bigdata.beginGroup("main");
    settings.remove("");
    bigdata.remove("");

    bigdata.setValue("searchHistory",QVariant::fromValue(searchHistory));
    bigdata.setValue("bookmarks",QVariant::fromValue(bookmarks));
    bigdata.setValue("history",QVariant::fromValue(mainHistory));
    bigdata.setValue("adblock",QVariant::fromValue(adblock));
    bigdata.setValue("atlHostHistory",QVariant::fromValue(atlHostHistory));
    bigdata.setValue("scpHostHistory",QVariant::fromValue(scpHostHistory));
    bigdata.setValue("userAgentHistory",QVariant::fromValue(userAgentHistory));
    bigdata.setValue("dictPaths",QVariant::fromValue(dictPaths));

    settings.setValue("hostingDir",hostingDir);
    settings.setValue("hostingUrl",hostingUrl);
    settings.setValue("maxLimit",maxLimit);
    settings.setValue("maxHistory",maxHistory);
    settings.setValue("browser",sysBrowser);
    settings.setValue("editor",sysEditor);
    settings.setValue("tr_engine",translatorEngine);
    settings.setValue("scp",useScp);
    settings.setValue("scphost",scpHost);
    settings.setValue("scpparams",scpParams);
    settings.setValue("atlasHost",atlHost);
    settings.setValue("atlasPort",atlPort);
    settings.setValue("auxDir",savedAuxDir);
    settings.setValue("emptyRestore",emptyRestore);
    settings.setValue("javascript",QWebEngineSettings::globalSettings()->
                      testAttribute(QWebEngineSettings::JavascriptEnabled));
    settings.setValue("autoloadimages",QWebEngineSettings::globalSettings()->
                      testAttribute(QWebEngineSettings::AutoLoadImages));
    settings.setValue("recycledCount",maxRecycled);

    settings.setValue("useAdblock",useAdblock);
    settings.setValue("useOverrideFont",useOverrideFont());
    settings.setValue("overrideFont",overrideFont.family());
    settings.setValue("overrideFontSize",overrideFont.pointSize());
    settings.setValue("overrideStdFonts",overrideStdFonts);
    settings.setValue("standardFont",fontStandard);
    settings.setValue("fixedFont",fontFixed);
    settings.setValue("serifFont",fontSerif);
    settings.setValue("sansSerifFont",fontSansSerif);
    settings.setValue("forceFontColor",forceFontColor());
    settings.setValue("forcedFontColor",forcedFontColor.name());
    settings.setValue("gctxHotkey",gctxTranHotkey->shortcut().toString());

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
    settings.setValue("showFavicons",showFavicons);
    settings.endGroup();
    bigdata.endGroup();
    settingsSaveMutex.unlock();
}

void CGlobalControl::readSettings()
{
    QSettings settings("kernel1024", "jpreader");
    QSettings bigdata("kernel1024", "jpreader-bigdata");
    settings.beginGroup("MainWindow");
    bigdata.beginGroup("main");

    int cnt = 0;
    QStringList qs;

    if (settings.value("searchCnt",0).toInt()>0) {
        cnt = settings.value("searchCnt",0).toInt();
        QStringList qs;
        for (int i=0;i<cnt;i++) {
            QString s=settings.value(QString("searchTxt%1").arg(i),"").toString();
            if (!s.isEmpty()) qs << s;
        }
        searchHistory.clear();
        searchHistory.append(qs);
    } else
        searchHistory = bigdata.value("searchHistory",QStringList()).value<QStringList>();

    updateAllQueryLists();

    if (settings.value("atlHost_count",0).toInt()>0) {
        cnt = settings.value("atlHost_count",0).toInt();
        qs.clear();
        atlHostHistory.clear();
        for (int i=0;i<cnt;i++) {
            QString s=settings.value(QString("atlHost%1").arg(i),"").toString();
            if (!s.isEmpty()) qs << s;
        }
        atlHostHistory.append(qs);
    } else
        atlHostHistory = bigdata.value("atlHostHistory",QStringList()).value<QStringList>();

    if (settings.value("scpHost_count",0).toInt()>0) {
        cnt = settings.value("scpHost_count",0).toInt();
        qs.clear();
        scpHostHistory.clear();
        for (int i=0;i<cnt;i++) {
            QString s=settings.value(QString("scpHost%1").arg(i),"").toString();
            if (!s.isEmpty()) qs << s;
        }
        scpHostHistory.append(qs);
    } else
        scpHostHistory = bigdata.value("scpHostHistory",QStringList()).value<QStringList>();

    int sz = settings.beginReadArray("bookmarks");
    if (sz>0) {
        for (int i=0; i<sz; i++) {
            settings.setArrayIndex(i);
            QString t = settings.value("title").toString();
            if (!t.isEmpty())
                bookmarks[t]=settings.value("url").toUrl();
        }
    } else
        bookmarks = bigdata.value("bookmarks").value<QBookmarksMap>();
    settings.endArray();

    QByteArray ba = settings.value("history",QByteArray()).toByteArray();
    if (!ba.isEmpty()) {
        mainHistory.clear();
        QDataStream hss(&ba,QIODevice::ReadOnly);
        try {
            hss >> mainHistory;
        } catch (...) {
            mainHistory.clear();
        }
    } else
        mainHistory = bigdata.value("history").value<QUHList>();

    adblockMutex.lock();
    if (settings.value("adblock_count",0).toInt()) {
        cnt = settings.value("adblock_count",0).toInt();
        adblock.clear();
        for (int i=0;i<cnt;i++) {
            QString at = settings.value(QString("adblock%1").arg(i),"").toString();
            QString it = settings.value(QString("adblock_listID%1").arg(i),"").toString();
            if (!at.isEmpty()) adblock << CAdBlockRule(at,it);
        }
    } else
        adblock = bigdata.value("adblock").value<CAdBlockList>();
    adblockMutex.unlock();

    if (settings.value("userAgentHistory_count",0).toInt()>0) {
        cnt=settings.value("userAgentHistory_count",0).toInt();
        userAgentHistory.clear();
        for (int i=0;i<cnt;i++) {
            QString s = settings.value(QString("userAgentHistory%1").arg(i),QString()).toString();
            if (!s.isEmpty())
                userAgentHistory << s;
        }
    } else
        userAgentHistory = bigdata.value("userAgentHistory",QStringList()).value<QStringList>();

    if (settings.value("dictPaths_count",0).toInt()>0) {
        cnt=settings.value("dictPaths_count",0).toInt();
        dictPaths.clear();
        for (int i=0;i<cnt;i++)
            dictPaths << settings.value(QString("dictPaths%1").arg(i)).toString();
    } else
        dictPaths = bigdata.value("dictPaths",QStringList()).value<QStringList>();

    hostingDir = settings.value("hostingDir","").toString();
    hostingUrl = settings.value("hostingUrl","about:blank").toString();
    maxLimit = settings.value("maxLimit",1000).toInt();
    maxHistory = settings.value("maxHistory",5000).toInt();
    sysBrowser = settings.value("browser","konqueror").toString();
    sysEditor = settings.value("editor","kwrite").toString();
    translatorEngine = settings.value("tr_engine",TE_ATLAS).toInt();
    useScp = settings.value("scp",false).toBool();
    scpHost = settings.value("scphost","").toString();
    scpParams = settings.value("scpparams","").toString();
    atlHost = settings.value("atlasHost","localhost").toString();
    atlPort = settings.value("atlasPort",18000).toInt();
    savedAuxDir = settings.value("auxDir",QDir::homePath()).toString();
    emptyRestore = settings.value("emptyRestore",false).toBool();
    maxRecycled = settings.value("recycledCount",20).toInt();
    bool jsstate = settings.value("javascript",true).toBool();
    QWebEngineSettings::globalSettings()->
            setAttribute(QWebEngineSettings::JavascriptEnabled,jsstate);
    actionJSUsage->setChecked(jsstate);
    QWebEngineSettings::globalSettings()->
            setAttribute(QWebEngineSettings::AutoLoadImages,
                                                 settings.value("autoloadimages",true).toBool());
    useAdblock=settings.value("useAdblock",false).toBool();
    actionOverrideFont->setChecked(settings.value("useOverrideFont",false).toBool());
    overrideFont.setFamily(settings.value("overrideFont","Verdana").toString());
    overrideFont.setPointSize(settings.value("overrideFontSize",12).toInt());
    overrideStdFonts=settings.value("overrideStdFonts",false).toBool();
    fontStandard=settings.value("standardFont",QApplication::font().family()).toString();
    fontFixed=settings.value("fixedFont","Courier New").toString();
    fontSerif=settings.value("serifFont","Times New Roman").toString();
    fontSansSerif=settings.value("sansSerifFont","Verdana").toString();
    actionOverrideFontColor->setChecked(settings.value("forceFontColor",false).toBool());
    forcedFontColor=QColor(settings.value("forcedFontColor","#000000").toString());
    QString hk = settings.value("gctxHotkey",QString()).toString();
    if (!hk.isEmpty()) {
        gctxTranHotkey->setShortcut(QKeySequence::fromString(hk));
        if (!gctxTranHotkey->shortcut().isEmpty())
            gctxTranHotkey->setEnabled();
    }
    searchEngine = settings.value("searchEngine",SE_NONE).toInt();
    atlTcpRetryCount = settings.value("atlTcpRetryCount",3).toInt();
    atlTcpTimeout = settings.value("atlTcpTimeout",2).toInt();
    showTabCloseButtons = settings.value("showTabCloseButtons",true).toBool();
    proxyHost = settings.value("proxyHost",QString()).toString();
    proxyPort = settings.value("proxyPort",3128).toInt();
    proxyType = static_cast<QNetworkProxy::ProxyType>(settings.value("proxyType",
                                                                     QNetworkProxy::HttpCachingProxy).toInt());
    proxyLogin = settings.value("proxyLogin",QString()).toString();
    proxyPassword = settings.value("proxyPassword",QString()).toString();
    proxyUse = settings.value("proxyUse",false).toBool();
    proxyUseTranslator = settings.value("proxyUseTranslator",false).toBool();
    bingID = settings.value("bingID",QString()).toString();
    bingKey = settings.value("bingKey",QString()).toString();
    yandexKey = settings.value("yandexKey",QString()).toString();
    createCoredumps = settings.value("createCoredumps",false).toBool();
    ignoreSSLErrors = settings.value("ignoreSSLErrors",false).toBool();
    showFavicons = settings.value("showFavicons",true).toBool();

    overrideUserAgent=settings.value("overrideUserAgent",false).toBool();
    userAgent=settings.value("userAgent",QString()).toString();
    if (userAgent.isEmpty())
        overrideUserAgent=false;
    else if (userAgentHistory.isEmpty())
        userAgentHistory << userAgent;

    if (overrideUserAgent)
        webProfile->setHttpUserAgent(userAgent);


    settings.endGroup();
    bigdata.endGroup();
    if (hostingDir.right(1)!="/") hostingDir=hostingDir+"/";
    if (hostingUrl.right(1)!="/") hostingUrl=hostingUrl+"/";
    updateAllBookmarks();
    updateProxy(proxyUse,true);
}

void CGlobalControl::writeTabsList(bool clearList)
{
    QList<QUrl> urls;
    urls.clear();
    if (!clearList) {
        for (int i=0;i<mainWindows.count();i++) {
            for (int j=0;j<mainWindows.at(i)->tabMain->count();j++) {
                CSnippetViewer* sn = qobject_cast<CSnippetViewer *>(mainWindows.at(i)->tabMain->widget(j));
                if (sn==NULL) continue;
                QUrl url = sn->getUrl();
                if (url.isValid() && !url.isEmpty())
                    urls << url;
            }
        }
        if (urls.isEmpty()) return;
    }

    QSettings settings("kernel1024", "jpreader-tabs");
    settings.beginGroup("OpenedTabs");
    settings.setValue("tabsCnt", urls.count());
    for (int i=0;i<urls.count();i++)
        settings.setValue(QString("tab_%1").arg(i),urls.at(i));
    settings.endGroup();
}

void CGlobalControl::checkRestoreLoad(CMainWindow *w)
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

void CGlobalControl::settingsDlg()
{
    if (dlg!=NULL) {
        dlg->activateWindow();
        return;
    }
    dlg = new CSettingsDlg();
    dlg->hostingDir->setText(hostingDir);
    dlg->hostingUrl->setText(hostingUrl);
    dlg->maxLimit->setValue(maxLimit);
    dlg->maxHistory->setValue(maxHistory);
    dlg->editor->setText(sysEditor);
    dlg->browser->setText(sysBrowser);
    dlg->maxRecycled->setValue(maxRecycled);
    dlg->useJS->setChecked(QWebEngineSettings::globalSettings()->
                           testAttribute(QWebEngineSettings::JavascriptEnabled));
    dlg->autoloadImages->setChecked(QWebEngineSettings::globalSettings()->
                                    testAttribute(QWebEngineSettings::AutoLoadImages));

    dlg->setBookmarks(bookmarks);
    dlg->setMainHistory(mainHistory);
    dlg->setQueryHistory(searchHistory);
    dlg->setAdblock(adblock);

    switch (translatorEngine) {
        case TE_GOOGLE: dlg->rbGoogle->setChecked(true); break;
        case TE_ATLAS: dlg->rbAtlas->setChecked(true); break;
        case TE_BINGAPI: dlg->rbBingAPI->setChecked(true); break;
        case TE_YANDEX: dlg->rbYandexAPI->setChecked(true); break;
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
    dlg->bingID->setText(bingID);
    dlg->bingKey->setText(bingKey);
    dlg->yandexKey->setText(yandexKey);
    dlg->emptyRestore->setChecked(emptyRestore);

    dlg->useAd->setChecked(useAdblock);
    dlg->useOverrideFont->setChecked(useOverrideFont());
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
    dlg->fontOverride->setEnabled(useOverrideFont());
    dlg->fontOverrideSize->setEnabled(useOverrideFont());
    dlg->fontStandard->setEnabled(overrideStdFonts);
    dlg->fontFixed->setEnabled(overrideStdFonts);
    dlg->fontSerif->setEnabled(overrideStdFonts);
    dlg->fontSansSerif->setEnabled(overrideStdFonts);
    dlg->overrideFontColor->setChecked(forceFontColor());
    dlg->updateFontColorPreview(forcedFontColor);
    dlg->gctxHotkey->setKeySequence(gctxTranHotkey->shortcut());
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

    dlg->debugLogNetReq->setChecked(actionLogNetRequests->isChecked());

    dlg->overrideUserAgent->setChecked(overrideUserAgent);
    dlg->userAgent->setEnabled(overrideUserAgent);
    dlg->userAgent->clear();
    dlg->userAgent->addItems(userAgentHistory);
    dlg->userAgent->lineEdit()->setText(userAgent);

    dlg->dictPaths->clear();
    dlg->dictPaths->addItems(dictPaths);
    dlg->loadedDicts.clear();
    dlg->loadedDicts.append(dictManager->getLoadedDictionaries());

    dlg->ignoreSSLErrors->setChecked(ignoreSSLErrors);
    dlg->visualShowFavicons->setChecked(showFavicons);
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
        default: dlg->proxyType->setCurrentIndex(0); break;
    }

    dlg->getCookiesFromStore();

    if (dlg->exec()==QDialog::Accepted) {
        hostingDir=dlg->hostingDir->text();
        hostingUrl=dlg->hostingUrl->text();
        maxLimit=dlg->maxLimit->value();
        maxHistory=dlg->maxHistory->value();
        sysEditor=dlg->editor->text();
        sysBrowser=dlg->browser->text();
        maxRecycled=dlg->maxRecycled->value();

        searchHistory.clear();
        searchHistory.append(dlg->getQueryHistory());
        updateAllQueryLists();
        bookmarks = dlg->getBookmarks();
        updateAllBookmarks();
        adblockMutex.lock();
        adblock.clear();
        adblock.append(dlg->getAdblock());
        adblockMutex.unlock();

        if (hostingDir.right(1)!="/") hostingDir=hostingDir+"/";
        if (hostingUrl.right(1)!="/") hostingUrl=hostingUrl+"/";

        QWebEngineSettings::globalSettings()->setAttribute(QWebEngineSettings::JavascriptEnabled,
                                                     dlg->useJS->isChecked());
        actionJSUsage->setChecked(dlg->useJS->isChecked());
        QWebEngineSettings::globalSettings()->setAttribute(QWebEngineSettings::AutoLoadImages,
                                                     dlg->autoloadImages->isChecked());
        if (dlg->rbGoogle->isChecked()) translatorEngine=TE_GOOGLE;
        else if (dlg->rbAtlas->isChecked()) translatorEngine=TE_ATLAS;
        else if (dlg->rbBingAPI->isChecked()) translatorEngine=TE_BINGAPI;
        else if (dlg->rbYandexAPI->isChecked()) translatorEngine=TE_YANDEX;
        else translatorEngine=TE_ATLAS;
        useScp=dlg->cbSCP->isChecked();
        scpHost=dlg->scpHost->lineEdit()->text();
        scpParams=dlg->scpParams->text();
        atlHost=dlg->atlHost->lineEdit()->text();
        atlPort=dlg->atlPort->value();
        atlTcpRetryCount=dlg->atlRetryCount->value();
        atlTcpTimeout=dlg->atlRetryTimeout->value();
        bingID=dlg->bingID->text();
        bingKey=dlg->bingKey->text();
        yandexKey=dlg->yandexKey->text();
        emptyRestore=dlg->emptyRestore->isChecked();
        if (scpHostHistory.contains(scpHost))
            scpHostHistory.move(scpHostHistory.indexOf(scpHost),0);
        else
            scpHostHistory.prepend(scpHost);
        if (atlHostHistory.contains(atlHost))
            atlHostHistory.move(atlHostHistory.indexOf(atlHost),0);
        else
            atlHostHistory.prepend(atlHost);
        actionOverrideFont->setChecked(dlg->useOverrideFont->isChecked());
        overrideStdFonts=dlg->overrideStdFonts->isChecked();
        overrideFont=dlg->fontOverride->currentFont();
        overrideFont.setPointSize(dlg->fontOverrideSize->value());
        fontStandard=dlg->fontStandard->currentFont().family();
        fontFixed=dlg->fontFixed->currentFont().family();
        fontSerif=dlg->fontSerif->currentFont().family();
        fontSansSerif=dlg->fontSansSerif->currentFont().family();
        actionOverrideFontColor->setChecked(dlg->overrideFontColor->isChecked());
        forcedFontColor=dlg->getOverridedFontColor();

        useAdblock=dlg->useAd->isChecked();

        gctxTranHotkey->setShortcut(dlg->gctxHotkey->keySequence());
        if (!gctxTranHotkey->shortcut().isEmpty())
            gctxTranHotkey->setEnabled();
        createCoredumps=dlg->createCoredumps->isChecked();
        if (dlg->searchRecoll->isChecked())
            searchEngine = SE_RECOLL;
        else if (dlg->searchBaloo5->isChecked())
            searchEngine = SE_BALOO5;
        else
            searchEngine = SE_NONE;

        showTabCloseButtons = dlg->visualShowTabCloseButtons->isChecked();

        actionLogNetRequests->setChecked(dlg->debugLogNetReq->isChecked());

        overrideUserAgent = dlg->overrideUserAgent->isChecked();
        if (overrideUserAgent) {
            userAgent = dlg->userAgent->lineEdit()->text();
            if (!userAgentHistory.contains(userAgent))
                userAgentHistory << userAgent;

            if (userAgent.isEmpty())
                overrideUserAgent=false;
        }

        if (overrideUserAgent)
            webProfile->setHttpUserAgent(userAgent);

        QStringList sl;
        sl.clear();
        for (int i=0;i<dlg->dictPaths->count();i++)
            sl.append(dlg->dictPaths->item(i)->text());
        if (compareStringLists(dictPaths,sl)!=0) {
            dictPaths.clear();
            dictPaths.append(sl);
            dictManager->loadDictionaries();
        }

        ignoreSSLErrors = dlg->ignoreSSLErrors->isChecked();
        showFavicons = dlg->visualShowFavicons->isChecked();
        proxyHost = dlg->proxyHost->text();
        proxyPort = dlg->proxyPort->value();
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

        updateProxy(proxyUse,true);
        emit settingsUpdated();
    }
    dlg->setParent(NULL);
    delete dlg;
    dlg=NULL;
}

void CGlobalControl::updateProxy(bool useProxy, bool forceMenuUpdate)
{
    proxyUse = useProxy;

    QNetworkProxy proxy = QNetworkProxy();

    if (proxyUse && !proxyHost.isEmpty())
        proxy = QNetworkProxy(proxyType,proxyHost,proxyPort,proxyLogin,proxyPassword);

    QNetworkProxy::setApplicationProxy(proxy);

    if (forceMenuUpdate)
        actionUseProxy->setChecked(proxyUse);
}

void CGlobalControl::toggleJSUsage(bool useJS)
{
    QWebEngineSettings::globalSettings()->setAttribute(QWebEngineSettings::JavascriptEnabled,useJS);
}

void CGlobalControl::toggleAutoloadImages(bool loadImages)
{
    QWebEngineSettings::globalSettings()->setAttribute(QWebEngineSettings::AutoLoadImages,loadImages);
}

void CGlobalControl::toggleLogNetRequests(bool logRequests)
{
    debugNetReqLogging = logRequests;
}

void CGlobalControl::cookieAdded(const QNetworkCookie &cookie)
{
    if (auxNetManager->cookieJar()!=NULL)
        auxNetManager->cookieJar()->insertCookie(cookie);
}

void CGlobalControl::cookieRemoved(const QNetworkCookie &cookie)
{
    if (auxNetManager->cookieJar()!=NULL)
        auxNetManager->cookieJar()->deleteCookie(cookie);
}

void CGlobalControl::cleanTmpFiles()
{
    for (int i=0; i<createdFiles.count(); i++) {
        QFile f(createdFiles[i]);
        f.remove();
    }
}

void CGlobalControl::closeLockTimer()
{
    blockTabCloseActive=false;
}

void CGlobalControl::blockTabClose()
{
    blockTabCloseActive=true;
    QTimer::singleShot(500,this,SLOT(closeLockTimer()));
}

void CGlobalControl::clipboardChanged(QClipboard::Mode mode)
{
    QClipboard *cb = QApplication::clipboard();

    if (actionGlobalTranslator->isChecked() &&
        (mode==QClipboard::Selection) &&
        (!cb->mimeData(QClipboard::Selection)->text().isEmpty()))
        startGlobalContextTranslate(cb->mimeData(QClipboard::Selection)->text());
}

void CGlobalControl::startGlobalContextTranslate(const QString &text)
{
    if (text.isEmpty()) return;
    QThread *th = new QThread();
    CAuxTranslator *at = new CAuxTranslator();
    at->setParams(text);
    connect(this,SIGNAL(startAuxTranslation()),
            at,SLOT(startTranslation()),Qt::QueuedConnection);
    connect(at,SIGNAL(gotTranslation(QString)),
            this,SLOT(globalContextTranslateReady(QString)),Qt::QueuedConnection);
    at->moveToThread(th);
    th->start();

    emit startAuxTranslation();
}

void CGlobalControl::globalContextTranslateReady(const QString &text)
{
    CSpecToolTipLabel* t = new CSpecToolTipLabel(wordWrap(text,80));
    t->setStyleSheet("QLabel { background: #fefdeb; color: black; }");
    QPoint p = QCursor::pos();
    QxtToolTip::show(p,t,NULL);
}

void CGlobalControl::showDictionaryWindow(const QString &text)
{
    if (auxDictionary==NULL) {
        auxDictionary = new CAuxDictionary();
        auxDictionary->show();
        auxDictionary->adjustSplitters();
    }

    if (!text.isEmpty())
        auxDictionary->findWord(text);
    else
        auxDictionary->restoreWindow();
}

void CGlobalControl::preShutdown()
{
    writeSettings();
    cleanTmpFiles();
    cleanupAndExit(false);
}

void CGlobalControl::appendSearchHistory(QStringList req)
{
    for(int i=0;i<req.count();i++) {
        int idx = searchHistory.indexOf(req.at(i));
        if (idx>=0)
            searchHistory.removeAt(idx);
        searchHistory.insert(0,req.at(i));
    }
}

void CGlobalControl::appendRecycled(QString title, QUrl url)
{
    int idx = recycleBin.indexOf(UrlHolder(title,url));
    if (idx>=0)
        recycleBin.move(idx,0);
    else
        recycleBin.prepend(UrlHolder(title,url));

    if (recycleBin.count()>maxRecycled) recycleBin.removeLast();

    updateAllRecycleBins();
}

void CGlobalControl::appendMainHistory(UrlHolder &item)
{
    if (mainHistory.contains(item))
        mainHistory.removeOne(item);
    mainHistory.prepend(item);

    while (mainHistory.count()>maxHistory)
        mainHistory.removeLast();

    updateAllHistoryLists();
}

bool CGlobalControl::updateMainHistoryTitle(UrlHolder &item, QString newTitle)
{
    if (mainHistory.contains(item)) {
        int idx = mainHistory.indexOf(item);
        if (idx>=0) {
            mainHistory[idx].title = newTitle;
            updateAllHistoryLists();
            return true;
        }
    }
    return false;
}

void CGlobalControl::updateAllBookmarks()
{
    foreach (CMainWindow* w, mainWindows) w->updateBookmarks();
}

void CGlobalControl::updateAllCharsetLists()
{
    foreach (CMainWindow* w, mainWindows) w->reloadCharsetList();
}

void CGlobalControl::updateAllHistoryLists()
{
    foreach (CMainWindow* w, mainWindows) w->updateHistoryList();
}

void CGlobalControl::updateAllQueryLists()
{
    foreach (CMainWindow* w, mainWindows) w->updateQueryHistory();
}

void CGlobalControl::updateAllRecycleBins()
{
    foreach (CMainWindow* w, mainWindows) w->updateRecycled();
}

void CGlobalControl::ipcMessageReceived()
{
    QLocalSocket *socket = ipcServer->nextPendingConnection();
    QByteArray bmsg;
    do {
        if (!socket->waitForReadyRead(2000)) return;
        bmsg.append(socket->readAll());
    } while (!bmsg.contains(QString(IPC_EOF).toUtf8()));
    socket->close();
    socket->deleteLater();

    QStringList cmd = QString::fromUtf8(bmsg).split('\n');
    if (cmd.first().startsWith("newWindow"))
        addMainWindow();
}

CMainWindow* CGlobalControl::addMainWindow(bool withSearch, bool withViewer)
{
    CMainWindow* mainWindow = new CMainWindow(withSearch,withViewer);
    connect(mainWindow,SIGNAL(aboutToClose(CMainWindow*)),this,SLOT(windowDestroyed(CMainWindow*)));

    mainWindows.append(mainWindow);

    mainWindow->show();

    mainWindow->menuTools->addAction(actionLogNetRequests);
    mainWindow->menuTools->addSeparator();
    mainWindow->menuTools->addAction(actionGlobalTranslator);
    mainWindow->menuTools->addAction(actionSelectionDictionary);
    mainWindow->menuTools->addAction(actionUseProxy);
    mainWindow->menuTools->addAction(actionSnippetAutotranslate);
    mainWindow->menuTools->addSeparator();
    mainWindow->menuTools->addAction(actionJSUsage);

    mainWindow->menuSettings->addAction(actionAutoTranslate);
    mainWindow->menuSettings->addAction(actionAutoloadImages);
    mainWindow->menuSettings->addAction(actionOverrideFont);
    mainWindow->menuSettings->addAction(actionOverrideFontColor);

    mainWindow->menuTranslationMode->addAction(actionTMAdditive);
    mainWindow->menuTranslationMode->addAction(actionTMOverwriting);
    mainWindow->menuTranslationMode->addAction(actionTMTooltip);

    mainWindow->menuSourceLanguage->addAction(actionLSJapanese);
    mainWindow->menuSourceLanguage->addAction(actionLSChineseSimplified);
    mainWindow->menuSourceLanguage->addAction(actionLSChineseTraditional);
    mainWindow->menuSourceLanguage->addAction(actionLSKorean);

    checkRestoreLoad(mainWindow);

    return mainWindow;
}

void CGlobalControl::focusChanged(QWidget *, QWidget *now)
{
    if (now==NULL) return;
    CMainWindow* mw = qobject_cast<CMainWindow*>(now->window());
    if (mw==NULL) return;
    activeWindow=mw;
}

void CGlobalControl::windowDestroyed(CMainWindow *obj)
{
    if (obj==NULL) return;
    mainWindows.removeOne(obj);
    if (activeWindow==obj) {
        if (mainWindows.count()>0) {
            activeWindow=mainWindows.first();
            activeWindow->activateWindow();
        } else {
            activeWindow=NULL;
            cleanupAndExit(true);
        }
    }
}

void CGlobalControl::cleanupAndExit(bool appQuit)
{
    if (cleaningState) return;
    cleaningState = true;

    writeTabsList(true);

    if (receivers(SIGNAL(stopTranslators()))>0)
        emit stopTranslators();

    if (mainWindows.count()>0) {
        foreach (CMainWindow* w, mainWindows) {
            if (w!=NULL)
                w->close();
            QApplication::processEvents();
        }
        QApplication::processEvents();
    }
    webProfile->deleteLater();
    QApplication::processEvents();
    gctxTranHotkey->setDisabled();
    QApplication::processEvents();
    gctxTranHotkey->deleteLater();
    QApplication::processEvents();

    ipcServer->close();
    QApplication::processEvents();

    webProfile=NULL;

    if (appQuit)
        QApplication::quit();
}

bool CGlobalControl::isUrlBlocked(QUrl url)
{
    if (!useAdblock) return false;
    if (!adblockMutex.tryLock(10000)) {
        qCritical() << "Failed to lock adblock mutex";
        return false;
    }
    QList<CAdBlockRule> adlist(adblock);
    adblockMutex.unlock();

    QString u = url.toString(QUrl::RemoveUserInfo | QUrl::RemovePort |
                             QUrl::RemoveFragment | QUrl::StripTrailingSlash);

    for(int i=0;i<adlist.count();i++) {
        if (adlist.at(i).networkMatch(u))
            return true;
    }
    return false;
}

void CGlobalControl::adblockAppend(QString url)
{
    adblockAppend(CAdBlockRule(url,QString()));
}

void CGlobalControl::adblockAppend(CAdBlockRule url)
{
    adblockMutex.lock();
    adblock.append(url);
    adblockMutex.unlock();
}

void CGlobalControl::adblockAppend(QList<CAdBlockRule> urls)
{
    adblockMutex.lock();
    adblock.append(urls);
    adblockMutex.unlock();
}

void CGlobalControl::readPassword(const QUrl &origin, QString &user, QString &password)
{
    if (!origin.isValid()) return;
    QSettings settings("kernel1024", "jpreader");
    settings.beginGroup("passwords");
    QString key = QString::fromLatin1(origin.toEncoded().toBase64());

    QString u = settings.value(QString("%1-user").arg(key),QString()).toString();
    QByteArray ba = settings.value(QString("%1-pass").arg(key),QByteArray()).toByteArray();
    QString p = "";
    if (!ba.isEmpty()) {
        p = QString::fromUtf8(QByteArray::fromBase64(ba));
    }

    user = "";
    password = "";
    if (!u.isNull() && !p.isNull()) {
        user = u;
        password = p;
    }
    settings.endGroup();
}

void CGlobalControl::savePassword(const QUrl &origin, const QString &user, const QString &password)
{
    if (!origin.isValid()) return;
    QSettings settings("kernel1024", "jpreader");
    settings.beginGroup("passwords");
    QString key = QString::fromLatin1(origin.toEncoded().toBase64());
    settings.setValue(QString("%1-user").arg(key),user);
    settings.setValue(QString("%1-pass").arg(key),password.toUtf8().toBase64());
    settings.endGroup();
}

int CGlobalControl::getTranslationMode()
{
    bool okconv;
    int res = 0;
    if (translationMode->checkedAction()!=NULL) {
        res = translationMode->checkedAction()->data().toInt(&okconv);
        if (!okconv)
            res = 0;
    }
    return res;
}

int CGlobalControl::getSourceLanguage()
{
    bool okconv;
    int res = 0;
    if (sourceLanguage->checkedAction()!=NULL) {
        res = sourceLanguage->checkedAction()->data().toInt(&okconv);
        if (!okconv)
            res = 0;
    }
    return res;
}

QString CGlobalControl::getSourceLanguageID(int engineStd)
{
    return getSourceLanguageIDStr(getSourceLanguage(),engineStd);
}

QString CGlobalControl::getSourceLanguageIDStr(int engine, int engineStd)
{
    QString srcLang;
    switch (engine) {
        case LS_JAPANESE: srcLang="ja"; break;
        case LS_CHINESETRAD:
            if (engineStd==TE_YANDEX)
                srcLang="zh";
            else if (engineStd==TE_BINGAPI)
                srcLang="zh-CHT";
            else
                srcLang="zh-TW";
            break;
        case LS_CHINESESIMP:
            if (engineStd==TE_YANDEX)
                srcLang="zh";
            else if (engineStd==TE_BINGAPI)
                srcLang="zh-CHS";
            else
                srcLang="zh-CN";
            break;
        case LS_KOREAN: srcLang="ko"; break;
        default: srcLang="ja"; break;
    }
    return srcLang;
}

QString CGlobalControl::getTranslationEngineString(int engine)
{
    switch (engine) {
        case TE_GOOGLE: return QString("Google");
        case TE_ATLAS: return QString("ATLAS");
        case TE_BINGAPI: return QString("Bing API");
        case TE_YANDEX: return QString("Yandex API");
        default: return QString("<unknown engine>");
    }
}

void CGlobalControl::setTranslationEngine(int engine)
{
    translatorEngine = engine;
    emit settingsUpdated();
}

UrlHolder::UrlHolder()
{
    UrlHolder::title="";
    UrlHolder::url=QUrl();
    UrlHolder::uuid=QUuid::createUuid();
}

UrlHolder::UrlHolder(QString title, QUrl url)
{
    UrlHolder::title=title;
    UrlHolder::url=url;
    UrlHolder::uuid=QUuid::createUuid();
}

UrlHolder& UrlHolder::operator=(const UrlHolder& other)
{
    title=other.title;
    url=other.url;
    uuid=other.uuid;
    return *this;
}

bool UrlHolder::operator==(const UrlHolder &s) const
{
    return (s.url==url);
}

bool UrlHolder::operator!=(const UrlHolder &s) const
{
    return !operator==(s);
}

DirStruct::DirStruct()
{
    DirStruct::dirName="";
    DirStruct::count=-1;
}

DirStruct::DirStruct(QString DirName, int Count)
{
    DirStruct::dirName=DirName;
    DirStruct::count=Count;
}

DirStruct& DirStruct::operator=(const DirStruct& other)
{
    dirName=other.dirName;
    count=other.count;
    return *this;
}

QDataStream &operator<<(QDataStream &out, const UrlHolder &obj) {
    out << obj.title << obj.url << obj.uuid;
    return out;
}

QDataStream &operator>>(QDataStream &in, UrlHolder &obj) {
    in >> obj.title >> obj.url >> obj.uuid;
    return in;
}


int compareStringLists(const QStringList &left, const QStringList &right)
{
    int diff = left.count()-right.count();
    if (diff!=0) return diff;

    for (int i=0;i<left.count();i++) {
        diff = QString::compare(left.at(i),right.at(i));
        if (diff!=0) return diff;
    }

    return 0;
}
