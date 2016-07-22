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
#include "miniqxt/qxttooltip.h"
#include "auxdictionary.h"
#include "downloadmanager.h"
#include "pdftotext.h"
#include "userscript.h"

#define IPC_EOF "\n###"

CGlobalControl::CGlobalControl(QApplication *parent) :
    QObject(parent)
{
    ipcServer = NULL;
    if (!setupIPC())
        return;

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

    appIcon.addFile(":/img/globe16");
    appIcon.addFile(":/img/globe32");
    appIcon.addFile(":/img/globe48");
    appIcon.addFile(":/img/globe128");

    userScripts.clear();
    logWindow = new CLogDisplay();

    downloadManager = new CDownloadManager();

    atlCerts.clear();

    activeWindow = NULL;
    lightTranslator = NULL;
    auxDictionary = NULL;

    initPdfToText();

    auxTranslatorDBus = new CAuxTranslator(this);
    new AuxtranslatorAdaptor(auxTranslatorDBus);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/",auxTranslatorDBus);
    dbus.registerService(DBUS_NAME);

    connect(parent,SIGNAL(focusChanged(QWidget*,QWidget*)),this,SLOT(focusChanged(QWidget*,QWidget*)));

    connect(ui.actionUseProxy,SIGNAL(toggled(bool)),
            this,SLOT(updateProxy(bool)));

    connect(ui.actionJSUsage,&QAction::toggled,[this](bool checked){
        webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,checked);
    });
    connect(ui.actionLogNetRequests,&QAction::toggled,[this](bool checked){
        settings.debugNetReqLogging = checked;
    });

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

    webProfile->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled,true);
    webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,true);

    connect(webProfile, SIGNAL(downloadRequested(QWebEngineDownloadItem*)),
            downloadManager, SLOT(handleDownload(QWebEngineDownloadItem*)));

    webProfile->setRequestInterceptor(new CSpecUrlInterceptor());
    webProfile->installUrlSchemeHandler(QByteArray("gdlookup"), new CSpecGDSchemeHandler());

    connect(webProfile->cookieStore(), SIGNAL(cookieAdded(QNetworkCookie)),
            this, SLOT(cookieAdded(QNetworkCookie)));
    connect(webProfile->cookieStore(), SIGNAL(cookieRemoved(QNetworkCookie)),
            this, SLOT(cookieRemoved(QNetworkCookie)));
    webProfile->cookieStore()->loadAllCookies();

    settings.readSettings(this);

    settings.dictIndexDir = fs + tr("dictIndex") + QDir::separator();
    QDir dictIndex(settings.dictIndexDir);
    if (!dictIndex.exists())
        if (!dictIndex.mkpath(settings.dictIndexDir)) {
            qCritical() << "Unable to create directory for dictionary indexes: " << settings.dictIndexDir;
            settings.dictIndexDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        }

    dictManager = new CGoldenDictMgr(this);

    connect(dictManager,&CGoldenDictMgr::showStatusBarMessage,[this](const QString& msg){
        if (activeWindow!=NULL) {
            if (msg.isEmpty())
                activeWindow->statusBar()->clearMessage();
            else
                activeWindow->statusBar()->showMessage(msg);
        }
    });
    connect(dictManager,&CGoldenDictMgr::showCriticalMessage,[this](const QString& msg){
        QMessageBox::critical(activeWindow,tr("JPReader"),msg);
    });

    dictNetMan = new ArticleNetworkAccessManager(this,dictManager);

    auxNetManager = new QNetworkAccessManager(this);
    auxNetManager->setCookieJar(new CNetworkCookieJar());

    tabsListTimer.setInterval(30000);
    connect(&tabsListTimer,SIGNAL(timeout()),&settings,SLOT(writeTabsList()));
    tabsListTimer.start();

    QTimer::singleShot(1500,dictManager,[this](){
        dictManager->loadDictionaries(settings.dictPaths, settings.dictIndexDir);
    });
}

bool CGlobalControl::setupIPC()
{
    QString serverName = IPC_NAME;
    serverName.replace(QRegExp("[^\\w\\-. ]"), "");

    QLocalSocket *socket = new QLocalSocket();
    socket->connectToServer(serverName);
    if (socket->waitForConnected(1000)){
        // Connected. Process is already running, send message to it
        if (runnedFromQtCreator()) { // This is debug run, try to close old instance
            // Send close request
            sendIPCMessage(socket,"debugRestart");
            socket->flush();
            socket->close();
            QApplication::processEvents();
            QThread::sleep(3);
            QApplication::processEvents();
            // Check for closing
            socket->connectToServer(serverName);
            if (socket->waitForConnected(1000)) { // connected, unable to close
                sendIPCMessage(socket,"newWindow");
                socket->flush();
                socket->close();
                socket->deleteLater();
                return false;
            }
            // Old instance closed, start new one
            ipcServer = new QLocalServer();
            ipcServer->removeServer(serverName);
            ipcServer->listen(serverName);
            connect(ipcServer, SIGNAL(newConnection()), this, SLOT(ipcMessageReceived()));
        } else {
            sendIPCMessage(socket,"newWindow");
            socket->flush();
            socket->close();
            socket->deleteLater();
            return false;
        }
    } else {
        // New process. Startup server and gSet normally, listen for new instances
        ipcServer = new QLocalServer();
        ipcServer->removeServer(serverName);
        ipcServer->listen(serverName);
        connect(ipcServer, SIGNAL(newConnection()), this, SLOT(ipcMessageReceived()));
    }
    if (socket->isOpen())
        socket->close();
    socket->deleteLater();
    return true;
}

void  CGlobalControl::sendIPCMessage(QLocalSocket *socket, const QString &msg)
{
    if (socket==NULL) return;

    QString s = QString("%1%2").arg(msg,IPC_EOF);
    socket->write(s.toUtf8());
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

void CGlobalControl::atlSSLCertErrors(const QSslCertificate &cert, const QStringList &errors,
                                      const QIntList &errCodes)
{
    if (atlCertErrorInteractive) return;

    QMessageBox mbox;
    mbox.setWindowTitle(tr("JPReader"));
    mbox.setText(tr("SSL error(s) occured while connecting to ATLAS service.\n"
                    "Add this certificate to trusted list?"));
    QString imsg;
    for(int i=0;i<errors.count();i++)
        imsg+=QString("%1. %2\n").arg(i+1).arg(errors.at(i));
    mbox.setInformativeText(imsg);

    mbox.setDetailedText(cert.toText());

    mbox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    mbox.setDefaultButton(QMessageBox::No);

    atlCertErrorInteractive = true;
    if (mbox.exec()==QMessageBox::Yes) {
        foreach (const int errCode, errCodes) {
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
    QString res = QUuid::createUuid().toString().remove(QRegExp("[^a-z,A-Z,0,1-9,-]"));
    if (!suffix.isEmpty())
        res.append(QString(".%1").arg(suffix));
    if (withDir)
        res = QDir().temp().absoluteFilePath(res);
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

    if (recycleBin.count()>settings.maxRecycled) recycleBin.removeLast();

    updateAllRecycleBins();
}

void CGlobalControl::appendMainHistory(UrlHolder &item)
{
    if (mainHistory.contains(item))
        mainHistory.removeOne(item);
    mainHistory.prepend(item);

    while (mainHistory.count()>settings.maxHistory)
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

void CGlobalControl::appendRecent(QString filename)
{
    QFileInfo fi(filename);
    if (!fi.exists()) return;

    if (recentFiles.contains(filename))
        recentFiles.removeAll(filename);
    recentFiles.prepend(filename);

    while (recentFiles.count()>settings.maxRecent)
        recentFiles.removeLast();

    updateAllRecentLists();
}

void CGlobalControl::updateAllRecentLists()
{
    foreach (CMainWindow* w, mainWindows) w->updateRecentList();
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
        ui.addMainWindow();
    else if (cmd.first().startsWith("debugRestart")) {
        qInfo() << QString("Closing jpreader instance (pid: %1) after debugRestart request")
                   .arg(QApplication::applicationPid());
        cleanupAndExit();
    }
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
            cleanupAndExit();
        }
    }
}

void CGlobalControl::cleanupAndExit()
{
    if (cleaningState) return;
    cleaningState = true;

    settings.writeTabsList(true);
    settings.writeSettings();
    cleanTmpFiles();

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
    ui.gctxTranHotkey.unsetShortcut();
    freePdfToText();
    webProfile->deleteLater();
    QApplication::processEvents();

    ipcServer->close();
    QApplication::processEvents();
    ipcServer->deleteLater();
    QApplication::processEvents();

    ipcServer=NULL;
    webProfile=NULL;

    QApplication::quit();
}

bool CGlobalControl::isUrlBlocked(QUrl url)
{
    QString dummy = QString();
    return isUrlBlocked(url,dummy);
}

bool CGlobalControl::isUrlBlocked(QUrl url, QString &filter)
{
    if (!settings.useAdblock) return false;

    filter.clear();
    QString u = url.toString(QUrl::RemoveUserInfo | QUrl::RemovePort |
                             QUrl::RemoveFragment | QUrl::StripTrailingSlash);

    foreach (const CAdBlockRule rule, adblock) {
        if (rule.networkMatch(u)) {
            filter = rule.filter();
            return true;
        }
    }
    return false;
}

void CGlobalControl::adblockAppend(QString url)
{
    adblockAppend(CAdBlockRule(url,QString()));
}

void CGlobalControl::adblockAppend(CAdBlockRule url)
{
    adblock.append(url);
}

void CGlobalControl::adblockAppend(QList<CAdBlockRule> urls)
{
    adblock.append(urls);
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

QList<CUserScript> CGlobalControl::getUserScriptsForUrl(const QUrl &url, bool isMainFrame)
{
    userScriptsMutex.lock();

    QList<CUserScript> scripts;
    QHash<QString, CUserScript>::iterator iterator;

    for (iterator = userScripts.begin(); iterator != userScripts.end(); ++iterator)
        if (iterator.value().isEnabledForUrl(url) &&
                (isMainFrame ||
                 (!isMainFrame && iterator.value().shouldRunOnSubFrames())))
            scripts.append(iterator.value());

    userScriptsMutex.unlock();

    return scripts;
}

void CGlobalControl::initUserScripts(const QStrHash &scripts)
{
    userScriptsMutex.lock();

    userScripts.clear();
    QStrHash::const_iterator iterator;
    for (iterator = scripts.begin(); iterator != scripts.end(); ++iterator)
        userScripts[iterator.key()] = CUserScript(iterator.key(), iterator.value());

    userScriptsMutex.unlock();
}

QStrHash CGlobalControl::getUserScripts()
{
    userScriptsMutex.lock();

    QStrHash res;
    QHash<QString, CUserScript>::iterator iterator;

    for (iterator = userScripts.begin(); iterator != userScripts.end(); ++iterator)
        res[iterator.key()] = iterator.value().getSource();

    userScriptsMutex.unlock();

    return res;
}

int CGlobalControl::getTranslationMode()
{
    bool okconv;
    int res = 0;
    if (ui.translationMode->checkedAction()!=NULL) {
        res = ui.translationMode->checkedAction()->data().toInt(&okconv);
        if (!okconv)
            res = 0;
    }
    return res;
}

int CGlobalControl::getSourceLanguage()
{
    bool okconv;
    int res = 0;
    if (ui.sourceLanguage->checkedAction()!=NULL) {
        res = ui.sourceLanguage->checkedAction()->data().toInt(&okconv);
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

void CGlobalControl::showLightTranslator(const QString &text)
{
    if (gSet->lightTranslator==NULL)
        gSet->lightTranslator = new CLightTranslator();

    gSet->lightTranslator->restoreWindow();

    if (!text.isEmpty())
        gSet->lightTranslator->appendSourceText(text);
}

void CGlobalControl::updateProxy(bool useProxy, bool forceMenuUpdate)
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
        return QUrl("about://blank");

    QString url = ctxSearchEngines.values().first();
    if (engine.isEmpty() && !settings.defaultSearchEngine.isEmpty())
        url = ctxSearchEngines.value(settings.defaultSearchEngine);
    if (!engine.isEmpty() && ctxSearchEngines.contains(engine))
        url = ctxSearchEngines.value(engine);

    url.replace("%s",text);
    url.replace("%ps",QUrl::toPercentEncoding(text));

    return QUrl::fromUserInput(url);
}
