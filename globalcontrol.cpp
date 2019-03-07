#include <QNetworkProxy>
#include <QMessageBox>
#include <QLocalSocket>
#include <QWebEngineSettings>
#include <QNetworkCookieJar>
#include <QStandardPaths>

#include <QWebEngineCookieStore>

#include "mainwindow.h"
#include "settingsdlg.h"
#include <goldendictlib/goldendictmgr.hh>

#include "globalcontrol.h"
#include "lighttranslator.h"
#include "auxtranslator.h"
#include "translator.h"
#include "genericfuncs.h"
#include "auxtranslator_adaptor.h"
#include "browsercontroller_adaptor.h"
#include "miniqxt/qxttooltip.h"
#include "auxdictionary.h"
#include "downloadmanager.h"
#include "pdftotext.h"
#include "userscript.h"

#define IPC_EOF "\n###"

CGlobalControl::CGlobalControl(QApplication *parent, int aInspectorPort) :
    QObject(parent)
{
    ipcServer = nullptr;
    if (!setupIPC())
        return;

    inspectorPort = aInspectorPort;

    atlCertErrorInteractive=false;

    blockTabCloseActive=false;
    adblock.clear();
    cleaningState=false;
    createdFiles.clear();
    recycleBin.clear();
    mainHistory.clear();
    searchHistory.clear();
    favicons.clear();
    ctxSearchEngines.clear();
    recentFiles.clear();
    adblockWhiteList.clear();
    noScriptWhiteList.clear();
    pageScriptHosts.clear();

    initLanguagesList();

    appIcon.addFile(QStringLiteral(":/img/globe16"));
    appIcon.addFile(QStringLiteral(":/img/globe32"));
    appIcon.addFile(QStringLiteral(":/img/globe48"));
    appIcon.addFile(QStringLiteral(":/img/globe128"));

    userScripts.clear();
    logWindow = new CLogDisplay();

    downloadManager = new CDownloadManager();
    bookmarksManager = new BookmarksManager(this);

    atlCerts.clear();

    activeWindow = nullptr;
    lightTranslator = nullptr;
    auxDictionary = nullptr;

    initPdfToText();

    auxTranslatorDBus = new CAuxTranslator(this);
    browserControllerDBus = new CBrowserController(this);
    new AuxtranslatorAdaptor(auxTranslatorDBus);
    new BrowsercontrollerAdaptor(browserControllerDBus);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.registerObject(QStringLiteral("/auxTranslator"),auxTranslatorDBus))
        qCritical() << dbus.lastError().name() << dbus.lastError().message();
    if (!dbus.registerObject(QStringLiteral("/browserController"),browserControllerDBus))
        qCritical() << dbus.lastError().name() << dbus.lastError().message();
    if (!dbus.registerService(QStringLiteral(DBUS_NAME)))
        qCritical() << dbus.lastError().name() << dbus.lastError().message();

    connect(parent, &QApplication::focusChanged, this, &CGlobalControl::focusChanged);

    connect(ui.actionUseProxy, &QAction::toggled, this, &CGlobalControl::updateProxy);

    connect(ui.actionJSUsage,&QAction::toggled,this,[this](bool checked){
        webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,checked);
    });
    connect(ui.actionLogNetRequests,&QAction::toggled,this,[this](bool checked){
        settings.debugNetReqLogging = checked;
    });

    webProfile = new QWebEngineProfile(QStringLiteral("jpreader"),this);

    QString fs = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    if (fs.isEmpty()) fs = QDir::homePath() + QDir::separator() + QStringLiteral(".cache")
                           + QDir::separator() + QStringLiteral("jpreader");
    if (!fs.endsWith(QDir::separator())) fs += QDir::separator();

    QString fcache = fs + QStringLiteral("cache") + QDir::separator();
    webProfile->setCachePath(fcache);
    QString fdata = fs + QStringLiteral("local_storage") + QDir::separator();
    webProfile->setPersistentStoragePath(fdata);

    webProfile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    webProfile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

#if QT_VERSION >= 0x050800
    webProfile->setSpellCheckEnabled(false);
#endif

    webProfile->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled,true);
    webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,true);

    connect(webProfile, &QWebEngineProfile::downloadRequested,
            downloadManager, &CDownloadManager::handleDownload);

    webProfile->setRequestInterceptor(new CSpecUrlInterceptor(this));

    connect(webProfile->cookieStore(), &QWebEngineCookieStore::cookieAdded,
            this, &CGlobalControl::cookieAdded);
    connect(webProfile->cookieStore(), &QWebEngineCookieStore::cookieRemoved,
            this, &CGlobalControl::cookieRemoved);
    webProfile->cookieStore()->loadAllCookies();

    settings.readSettings(this);

    settings.dictIndexDir = fs + QStringLiteral("dictIndex") + QDir::separator();
    QDir dictIndex(settings.dictIndexDir);
    if (!dictIndex.exists())
        if (!dictIndex.mkpath(settings.dictIndexDir)) {
            qCritical() << "Unable to create directory for dictionary indexes: " << settings.dictIndexDir;
            settings.dictIndexDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        }

    dictManager = new CGoldenDictMgr(this);

    connect(dictManager,&CGoldenDictMgr::showStatusBarMessage,this,[this](const QString& msg){
        if (activeWindow) {
            if (msg.isEmpty())
                activeWindow->statusBar()->clearMessage();
            else
                activeWindow->statusBar()->showMessage(msg);
        }
    });
    connect(dictManager,&CGoldenDictMgr::showCriticalMessage,this,[this](const QString& msg){
        QMessageBox::critical(activeWindow,tr("JPReader"),msg);
    });

    dictNetMan = new ArticleNetworkAccessManager(this,dictManager);

    auxNetManager = new QNetworkAccessManager(this);
    auxNetManager->setCookieJar(new CNetworkCookieJar());

    tabsListTimer.setInterval(30000);
    connect(&tabsListTimer, &QTimer::timeout, &settings, &CSettings::writeTabsList);
    tabsListTimer.start();

    QTimer::singleShot(1500,dictManager,[this](){
        dictManager->loadDictionaries(settings.dictPaths, settings.dictIndexDir);
    });
}

bool CGlobalControl::setupIPC()
{
    QString serverName = QStringLiteral(IPC_NAME);
    serverName.replace(QRegExp(QStringLiteral("[^\\w\\-. ]")), QString());

    auto socket = new QLocalSocket();
    socket->connectToServer(serverName);
    if (socket->waitForConnected(1000)){
        // Connected. Process is already running, send message to it
        if (runnedFromQtCreator()) { // This is debug run, try to close old instance
            // Send close request
            sendIPCMessage(socket,QStringLiteral("debugRestart"));
            socket->flush();
            socket->close();
            QApplication::processEvents();
            QThread::sleep(3);
            QApplication::processEvents();
            // Check for closing
            socket->connectToServer(serverName);
            if (socket->waitForConnected(1000)) { // connected, unable to close
                sendIPCMessage(socket,QStringLiteral("newWindow"));
                socket->flush();
                socket->close();
                socket->deleteLater();
                return false;
            }
            // Old instance closed, start new one
            QLocalServer::removeServer(serverName);
            ipcServer = new QLocalServer();
            ipcServer->listen(serverName);
            connect(ipcServer, &QLocalServer::newConnection, this, &CGlobalControl::ipcMessageReceived);
        } else {
            sendIPCMessage(socket,QStringLiteral("newWindow"));
            socket->flush();
            socket->close();
            socket->deleteLater();
            return false;
        }
    } else {
        // New process. Startup server and gSet normally, listen for new instances
        QLocalServer::removeServer(serverName);
        ipcServer = new QLocalServer();
        ipcServer->listen(serverName);
        connect(ipcServer, &QLocalServer::newConnection, this, &CGlobalControl::ipcMessageReceived);
    }
    if (socket->isOpen())
        socket->close();
    socket->deleteLater();
    return true;
}

void  CGlobalControl::sendIPCMessage(QLocalSocket *socket, const QString &msg)
{
    if (socket==nullptr) return;

    QString s = QStringLiteral("%1%2").arg(msg,QStringLiteral(IPC_EOF));
    socket->write(s.toUtf8());
}

void CGlobalControl::cookieAdded(const QNetworkCookie &cookie)
{
    if (auxNetManager->cookieJar())
        auxNetManager->cookieJar()->insertCookie(cookie);
}

void CGlobalControl::cookieRemoved(const QNetworkCookie &cookie)
{
    if (auxNetManager->cookieJar())
        auxNetManager->cookieJar()->deleteCookie(cookie);
}

void CGlobalControl::atlSSLCertErrors(const QSslCertificate &cert, const QStringList &errors,
                                      const CIntList &errCodes)
{
    if (atlCertErrorInteractive) return;

    QMessageBox mbox;
    mbox.setWindowTitle(tr("JPReader"));
    mbox.setText(tr("SSL error(s) occured while connecting to ATLAS service.\n"
                    "Add this certificate to trusted list?"));
    QString imsg;
    for(int i=0;i<errors.count();i++)
        imsg+=QStringLiteral("%1. %2\n").arg(i+1).arg(errors.at(i));
    mbox.setInformativeText(imsg);

    mbox.setDetailedText(cert.toText());

    mbox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    mbox.setDefaultButton(QMessageBox::No);

    atlCertErrorInteractive = true;
    if (mbox.exec()==QMessageBox::Yes) {
        for (const int errCode : qAsConst(errCodes)) {
            if (!gSet->atlCerts[cert].contains(errCode))
                gSet->atlCerts[cert].append(errCode);
        }
    }
    atlCertErrorInteractive = false;
}

void CGlobalControl::cleanTmpFiles()
{
    for (int i=0; i<createdFiles.count(); i++) {
        QFile f(createdFiles[i]);
        f.remove();
    }
}

QString CGlobalControl::makeTmpFileName(const QString& suffix, bool withDir)
{
    QString res = QUuid::createUuid().toString().remove(QRegExp(QStringLiteral("[^a-z,A-Z,0,1-9,-]")));
    if (!suffix.isEmpty())
        res.append(QStringLiteral(".%1").arg(suffix));
    if (withDir)
        res = QDir::temp().absoluteFilePath(res);
    return res;
}

QUrl CGlobalControl::getInspectorUrl() const
{
    QUrl res;
    if (inspectorPort<=0 || !qEnvironmentVariableIsSet("QTWEBENGINE_REMOTE_DEBUGGING"))
        return res;

    res = QUrl::fromUserInput(QString::fromUtf8(qgetenv("QTWEBENGINE_REMOTE_DEBUGGING")));
    return res;
}

void CGlobalControl::blockTabClose()
{
    blockTabCloseActive=true;
    QTimer::singleShot(500,this,[this](){
        blockTabCloseActive=false;
    });
}

void CGlobalControl::showDictionaryWindow()
{
    showDictionaryWindowEx(QString());
}

void CGlobalControl::showDictionaryWindowEx(const QString &text)
{
    if (auxDictionary==nullptr) {
        auxDictionary = new CAuxDictionary();
        auxDictionary->show();
        auxDictionary->adjustSplitters();
    }

    if (!text.isEmpty())
        auxDictionary->findWord(text);
    else
        auxDictionary->restoreWindow();
}

void CGlobalControl::appendSearchHistory(const QStringList& req)
{
    for(int i=0;i<req.count();i++) {
        int idx = searchHistory.indexOf(req.at(i));
        if (idx>=0)
            searchHistory.removeAt(idx);
        searchHistory.insert(0,req.at(i));
    }
}

void CGlobalControl::appendRecycled(const QString& title, const QUrl& url)
{
    int idx = recycleBin.indexOf(CUrlHolder(title,url));
    if (idx>=0)
        recycleBin.move(idx,0);
    else
        recycleBin.prepend(CUrlHolder(title,url));

    if (recycleBin.count()>settings.maxRecycled) recycleBin.removeLast();

    emit updateAllRecycleBins();
}

void CGlobalControl::appendMainHistory(const CUrlHolder &item)
{
    if (item.url.toString().startsWith(QStringLiteral("data:"),Qt::CaseInsensitive)) return;

    if (mainHistory.contains(item))
        mainHistory.removeOne(item);
    mainHistory.prepend(item);

    while (mainHistory.count()>settings.maxHistory)
        mainHistory.removeLast();

    emit updateAllHistoryLists();
}

bool CGlobalControl::updateMainHistoryTitle(const CUrlHolder &item, const QString& newTitle)
{
    if (mainHistory.contains(item)) {
        int idx = mainHistory.indexOf(item);
        if (idx>=0) {
            mainHistory[idx].title = newTitle;
            emit updateAllHistoryLists();
            return true;
        }
    }
    return false;
}

void CGlobalControl::appendRecent(const QString& filename)
{
    QFileInfo fi(filename);
    if (!fi.exists()) return;

    if (recentFiles.contains(filename))
        recentFiles.removeAll(filename);
    recentFiles.prepend(filename);

    while (recentFiles.count()>settings.maxRecent)
        recentFiles.removeLast();

    emit updateAllRecentLists();
}

void CGlobalControl::ipcMessageReceived()
{
    QLocalSocket *socket = ipcServer->nextPendingConnection();
    QByteArray bmsg;
    do {
        if (!socket->waitForReadyRead(2000)) return;
        bmsg.append(socket->readAll());
    } while (!bmsg.contains(QStringLiteral(IPC_EOF).toUtf8()));
    socket->close();
    socket->deleteLater();

    QStringList cmd = QString::fromUtf8(bmsg).split('\n');
    if (cmd.first().startsWith(QStringLiteral("newWindow")))
        ui.addMainWindow();
    else if (cmd.first().startsWith(QStringLiteral("debugRestart"))) {
        qInfo() << tr("Closing jpreader instance (pid: %1)"
                                          "after debugRestart request")
                   .arg(QApplication::applicationPid());
        cleanupAndExit();
    }
}

void CGlobalControl::focusChanged(QWidget *, QWidget *now)
{
    if (now==nullptr) return;
    auto mw = qobject_cast<CMainWindow*>(now->window());
    if (mw==nullptr) return;
    activeWindow=mw;
}

void CGlobalControl::windowDestroyed(CMainWindow *obj)
{
    if (obj==nullptr) return;
    mainWindows.removeOne(obj);
    if (activeWindow==obj) {
        if (mainWindows.count()>0) {
            activeWindow = mainWindows.constFirst();
            activeWindow->activateWindow();
        } else {
            activeWindow=nullptr;
            cleanupAndExit();
        }
    }
}

void CGlobalControl::cleanupAndExit()
{
    if (cleaningState) return;
    cleaningState = true;

    settings.clearTabsList();
    settings.writeSettings();
    cleanTmpFiles();

    emit stopTranslators();

    if (mainWindows.count()>0) {
        for (CMainWindow* w : qAsConst(mainWindows)) {
            if (w)
                w->close();
            QApplication::processEvents();
        }
        QApplication::processEvents();
    }
    ui.gctxTranHotkey.unsetShortcut();
    freePdfToText();
    webProfile->deleteLater();
    QApplication::processEvents();

    ipcServer->close();
    QApplication::processEvents();
    ipcServer->deleteLater();
    QApplication::processEvents();

    ipcServer=nullptr;
    webProfile=nullptr;

    QApplication::quit();
}

bool CGlobalControl::isUrlBlocked(const QUrl& url)
{
    QString dummy = QString();
    return isUrlBlocked(url,dummy);
}

bool CGlobalControl::isUrlBlocked(const QUrl& url, QString &filter)
{
    if (!settings.useAdblock) return false;
    if (!url.scheme().startsWith(QStringLiteral("http"),Qt::CaseInsensitive)) return false;

    filter.clear();
    QString u = url.toString(CSettings::adblockUrlFmt);

    if (adblockWhiteList.contains(u)) return false;

    for (const CAdBlockRule& rule : qAsConst(adblock)) {
        if (rule.networkMatch(u)) {
            filter = rule.filter();
            return true;
        }
    }

    // append unblocked url to cache
    adblockWhiteListMutex.lock();
    adblockWhiteList.append(u);
    while(adblockWhiteList.count()>settings.maxAdblockWhiteList)
        adblockWhiteList.removeFirst();
    adblockWhiteListMutex.unlock();

    return false;
}

bool CGlobalControl::isScriptBlocked(const QUrl &url, const QUrl& origin)
{
    if (!settings.useNoScript) return false;
    if (url.isRelative() || url.isLocalFile()) return false;

    const QString host = url.host();
    const QString key = origin.toString(CSettings::adblockUrlFmt);

    noScriptMutex.lock();
    pageScriptHosts[key].insert(host.toLower());
    bool block = !(noScriptWhiteList.contains(host));
    noScriptMutex.unlock();

    return block;
}

void CGlobalControl::adblockAppend(const QString& url)
{
    adblockAppend(CAdBlockRule(url,QString()));
}

void CGlobalControl::adblockAppend(const CAdBlockRule& url)
{
    QVector<CAdBlockRule> list;
    list << url;
    adblockAppend(list);
}

void CGlobalControl::adblockAppend(const QVector<CAdBlockRule> &urls)
{
    adblock.append(urls);

    adblockWhiteListMutex.lock();
    adblockWhiteList.clear();
    adblockWhiteListMutex.unlock();
}

void CGlobalControl::clearNoScriptPageHosts(const QString &origin)
{
    noScriptMutex.lock();
    pageScriptHosts[origin].clear();
    noScriptMutex.unlock();
}

CStringSet CGlobalControl::getNoScriptPageHosts(const QString &origin)
{
    CStringSet res;
    noScriptMutex.lock();
    res = pageScriptHosts.value(origin);
    noScriptMutex.unlock();
    return res;
}

void CGlobalControl::removeNoScriptWhitelistHost(const QString &host)
{
    noScriptMutex.lock();
    noScriptWhiteList.remove(host);
    noScriptMutex.unlock();
}

void CGlobalControl::addNoScriptWhitelistHost(const QString &host)
{
    noScriptMutex.lock();
    noScriptWhiteList.insert(host);
    noScriptMutex.unlock();
}

bool CGlobalControl::containsNoScriptWhitelist(const QString &host) const
{
    return noScriptWhiteList.contains(host);
}

bool CGlobalControl::haveSavedPassword(const QUrl &origin)
{
    QString user, pass;
    readPassword(origin, user, pass);
    return (!user.isEmpty() || !pass.isEmpty());
}

QUrl CGlobalControl::cleanUrlForRealm(const QUrl &origin) const
{
    QUrl res = origin;
    if (res.isEmpty() || !res.isValid() || res.isLocalFile() || res.isRelative()) {
        res = QUrl();
        return res;
    }

    if (res.hasFragment())
        res.setFragment(QString());
    if (res.hasQuery())
        res.setQuery(QString());

    return res;
}

void CGlobalControl::readPassword(const QUrl &origin, QString &user, QString &password)
{
    user = QString();
    password = QString();

    QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params(QStringLiteral("kernel1024"), QStringLiteral("jpreader"));
    params.beginGroup(QStringLiteral("passwords"));
    QString key = QString::fromLatin1(url.toEncoded().toBase64());

    QString u = params.value(QStringLiteral("%1-user").arg(key),QString()).toString();
    QByteArray ba = params.value(QStringLiteral("%1-pass").arg(key),QByteArray()).toByteArray();
    QString p;
    if (!ba.isEmpty()) {
        p = QString::fromUtf8(QByteArray::fromBase64(ba));
    }

    if (!u.isNull() && !p.isNull()) {
        user = u;
        password = p;
    }
    params.endGroup();
}

void CGlobalControl::savePassword(const QUrl &origin, const QString &user, const QString &password)
{
    QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params(QStringLiteral("kernel1024"), QStringLiteral("jpreader"));
    params.beginGroup(QStringLiteral("passwords"));
    QString key = QString::fromLatin1(url.toEncoded().toBase64());
    params.setValue(QStringLiteral("%1-user").arg(key),user);
    params.setValue(QStringLiteral("%1-pass").arg(key),password.toUtf8().toBase64());
    params.endGroup();
}

void CGlobalControl::removePassword(const QUrl &origin)
{
    QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params(QStringLiteral("kernel1024"), QStringLiteral("jpreader"));
    params.beginGroup(QStringLiteral("passwords"));
    QString key = QString::fromLatin1(url.toEncoded().toBase64());
    params.remove(QStringLiteral("%1-user").arg(key));
    params.remove(QStringLiteral("%1-pass").arg(key));
    params.endGroup();
}

QVector<CUserScript> CGlobalControl::getUserScriptsForUrl(const QUrl &url, bool isMainFrame)
{
    userScriptsMutex.lock();

    QVector<CUserScript> scripts;

    for (auto it = userScripts.constBegin(), end = userScripts.constEnd(); it != end; ++it)
        if (it.value().isEnabledForUrl(url) &&
                (isMainFrame || it.value().shouldRunOnSubFrames()))
            scripts.append(it.value());

    userScriptsMutex.unlock();

    return scripts;
}

void CGlobalControl::initUserScripts(const CStringHash &scripts)
{
    userScriptsMutex.lock();

    userScripts.clear();
    for (auto it = scripts.constBegin(), end = scripts.constEnd(); it != end; ++it)
        userScripts[it.key()] = CUserScript(it.key(), it.value());

    userScriptsMutex.unlock();
}

CStringHash CGlobalControl::getUserScripts()
{
    userScriptsMutex.lock();

    CStringHash res;

    for (auto it = userScripts.constBegin(), end = userScripts.constEnd(); it != end; ++it)
        res[it.key()] = (*it).getSource();

    userScriptsMutex.unlock();

    return res;
}

void CGlobalControl::initLanguagesList()
{
    const QList<QLocale> allLocales = QLocale::matchingLocales(
                QLocale::AnyLanguage,
                QLocale::AnyScript,
                QLocale::AnyCountry);

    langNamesList.clear();
    langSortedBCP47List.clear();

    for(const QLocale &locale : allLocales) {
        QString bcp = locale.bcp47Name();
        QString name = QStringLiteral("%1 (%2)")
                       .arg(QLocale::languageToString(locale.language()),bcp);

        // filter out unsupported codes for dialects
        if (bcp.contains('-') && !bcp.startsWith(QStringLiteral("zh"))) continue;

        // replace C locale with English
        if (bcp == QStringLiteral("en"))
            name = QStringLiteral("English (en)");

        if (!langNamesList.contains(bcp)) {
            langNamesList.insert(bcp,name);
            langSortedBCP47List.insert(name,bcp);
        }
    }
}

QString CGlobalControl::getLanguageName(const QString& bcp47Name)
{
    return langNamesList.value(bcp47Name);
}

QStringList CGlobalControl::getLanguageCodes() const
{
    return langSortedBCP47List.values();
}

QString CGlobalControl::getTranslationEngineString(int engine)
{
    switch (engine) {
        case TE_GOOGLE: return QStringLiteral("Google");
        case TE_ATLAS: return QStringLiteral("ATLAS");
        case TE_BINGAPI: return QStringLiteral("Bing API");
        case TE_YANDEX: return QStringLiteral("Yandex API");
        case TE_GOOGLE_GTX: return QStringLiteral("Google GTX");
        default: return QStringLiteral("<unknown engine>");
    }
}

void CGlobalControl::showLightTranslator(const QString &text)
{
    if (gSet->lightTranslator==nullptr)
        gSet->lightTranslator = new CLightTranslator();

    gSet->lightTranslator->restoreWindow();

    if (!text.isEmpty())
        gSet->lightTranslator->appendSourceText(text);
}

void CGlobalControl::updateProxy(bool useProxy)
{
    updateProxyWithMenuUpdate(useProxy, false);
}

void CGlobalControl::updateProxyWithMenuUpdate(bool useProxy, bool forceMenuUpdate)
{
    settings.proxyUse = useProxy;

    QNetworkProxy proxy = QNetworkProxy();

    if (settings.proxyUse && !settings.proxyHost.isEmpty())
        proxy = QNetworkProxy(settings.proxyType,settings.proxyHost,
                              settings.proxyPort,settings.proxyLogin,
                              settings.proxyPassword);

    QNetworkProxy::setApplicationProxy(proxy);

    if (forceMenuUpdate)
        ui.actionUseProxy->setChecked(settings.proxyUse);
}

void CGlobalControl::clearCaches()
{
    webProfile->clearHttpCache();
}

QUrl CGlobalControl::createSearchUrl(const QString& text, const QString& engine)
{
    if (ctxSearchEngines.isEmpty())
        return QUrl(QStringLiteral("about://blank"));

    auto it = ctxSearchEngines.constKeyValueBegin();
    QString url = (*it).second;
    if (engine.isEmpty() && !settings.defaultSearchEngine.isEmpty())
        url = ctxSearchEngines.value(settings.defaultSearchEngine);
    if (!engine.isEmpty() && ctxSearchEngines.contains(engine))
        url = ctxSearchEngines.value(engine);

    url.replace(QStringLiteral("%s"),text);
    url.replace(QStringLiteral("%ps"),QUrl::toPercentEncoding(text));

    return QUrl::fromUserInput(url);
}
