#include <QNetworkProxy>
#include <QMessageBox>
#include <QLocalSocket>
#include <QWebEngineSettings>
#include <QNetworkCookieJar>
#include <QStandardPaths>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <goldendictlib/goldendictmgr.hh>

extern "C" {
#include <sys/resource.h>
#include <unicode/uclean.h>
}

#include "mainwindow.h"
#include "settingsdlg.h"
#include "logdisplay.h"
#include "bookmarks.h"
#include "globalcontrol.h"
#include "lighttranslator.h"
#include "auxtranslator.h"
#include "translator.h"
#include "genericfuncs.h"
#include "auxtranslator_adaptor.h"
#include "browsercontroller.h"
#include "browsercontroller_adaptor.h"
#include "miniqxt/qxttooltip.h"
#include "auxdictionary.h"
#include "downloadmanager.h"
#include "pdftotext.h"
#include "userscript.h"
#include "structures.h"
#include "globalprivate.h"

namespace CDefaults {
const auto ipcEOF = "\n###";
const int tabListSavePeriod = 30000;
const int dictionariesLoadingDelay = 1500;
const int ipcTimeout = 1000;
const int tabCloseInterlockDelay = 500;
}

CGlobalControl::CGlobalControl(QCoreApplication *parent) :
    QObject(parent),
    dptr(new CGlobalControlPrivate(this)),
    m_settings(new CSettings(this)),
    m_ui(new CGlobalUI(this))
{

}

CGlobalControl::~CGlobalControl()
{
    Q_D(CGlobalControl);
    qInstallMessageHandler(nullptr);
    if (d->webProfile!=nullptr) {
        d->webProfile->setParent(nullptr);
        delete d->webProfile;
    }
    u_cleanup();
}

CGlobalControl *CGlobalControl::instance()
{
    static CGlobalControl* inst = nullptr;
    static QMutex lock;

    QMutexLocker locker(&lock);
    if (inst==nullptr)
        inst = new CGlobalControl(QApplication::instance());

    return inst;
}

QApplication *CGlobalControl::app(QObject *parentApp)
{
    QObject* capp = parentApp;
    if (capp==nullptr)
        capp = QApplication::instance();

    auto res = qobject_cast<QApplication *>(capp);
    return res;
}

void CGlobalControl::initialize()
{
    Q_D(CGlobalControl);

    int dbgport = CGenericFuncs::getRandomTCPPort();
    if (dbgport>0) {
        QString url = QStringLiteral("127.0.0.1:%1").arg(dbgport);
        setenv("QTWEBENGINE_REMOTE_DEBUGGING",url.toUtf8().constData(),1);
    }
    d->inspectorPort = dbgport;

    qInstallMessageHandler(CGlobalControlPrivate::stdConsoleOutput);
    qRegisterMetaType<CUrlHolder>("CUrlHolder");
    qRegisterMetaType<QDir>("QDir");
    qRegisterMetaType<QSslCertificate>("QSslCertificate");
    qRegisterMetaType<CIntList>("CIntList");
    qRegisterMetaType<CLangPair>("CLangPair");
    qRegisterMetaTypeStreamOperators<CUrlHolder>("CUrlHolder");
    qRegisterMetaTypeStreamOperators<CAdBlockRule>("CAdBlockRule");
    qRegisterMetaTypeStreamOperators<CAdBlockVector>("CAdBlockVector");
    qRegisterMetaTypeStreamOperators<CUrlHolderVector>("CUrlHolderVector");
    qRegisterMetaTypeStreamOperators<CStringHash>("CStringHash");
    qRegisterMetaTypeStreamOperators<CSslCertificateHash>("CSslCertificateHash");
    qRegisterMetaTypeStreamOperators<CLangPairVector>("CLangPairVector");
    qRegisterMetaTypeStreamOperators<CStringSet>("CStringSet");

    if (!setupIPC())
        ::exit(0);

    initLanguagesList();

    d->logWindow.reset(new CLogDisplay());
    d->downloadManager.reset(new CDownloadManager());
    d->bookmarksManager = new BookmarksManager(this);

    d->activeWindow = nullptr;

    CPDFWorker::initPdfToText();

    d->auxTranslatorDBus = new CAuxTranslator(this);
    d->browserControllerDBus = new CBrowserController(this);
    new AuxtranslatorAdaptor(d->auxTranslatorDBus);
    new BrowsercontrollerAdaptor(d->browserControllerDBus);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.registerObject(QStringLiteral("/auxTranslator"),d->auxTranslatorDBus))
        qCritical() << dbus.lastError().name() << dbus.lastError().message();
    if (!dbus.registerObject(QStringLiteral("/browserController"),d->browserControllerDBus))
        qCritical() << dbus.lastError().name() << dbus.lastError().message();
    if (!dbus.registerService(QString::fromLatin1(CDefaults::DBusName)))
        qCritical() << dbus.lastError().name() << dbus.lastError().message();

    connect(app(parent()), &QApplication::focusChanged, this, &CGlobalControl::focusChanged);

    connect(m_ui->actionUseProxy, &QAction::toggled, this, &CGlobalControl::updateProxy);

    connect(m_ui->actionJSUsage,&QAction::toggled,this,[d](bool checked){
        d->webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,checked);
    });
    connect(m_ui->actionLogNetRequests,&QAction::toggled,this,[this](bool checked){
        m_settings->debugNetReqLogging = checked;
    });

    d->webProfile = new QWebEngineProfile(QStringLiteral("jpreader"),this);

    QString fs = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    if (fs.isEmpty()) fs = QDir::homePath() + QDir::separator() + QStringLiteral(".cache")
                           + QDir::separator() + QStringLiteral("jpreader");
    if (!fs.endsWith(QDir::separator())) fs += QDir::separator();

    QString fcache = fs + QStringLiteral("cache") + QDir::separator();
    d->webProfile->setCachePath(fcache);
    QString fdata = fs + QStringLiteral("local_storage") + QDir::separator();
    d->webProfile->setPersistentStoragePath(fdata);

    d->webProfile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    d->webProfile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

    d->webProfile->setSpellCheckEnabled(false);

    d->webProfile->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled,true);
    d->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,true);

    connect(d->webProfile, &QWebEngineProfile::downloadRequested,
            d->downloadManager.data(), &CDownloadManager::handleDownload);

    d->webProfile->setRequestInterceptor(new CSpecUrlInterceptor(this));

    connect(d->webProfile->cookieStore(), &QWebEngineCookieStore::cookieAdded,
            this, &CGlobalControl::cookieAdded);
    connect(d->webProfile->cookieStore(), &QWebEngineCookieStore::cookieRemoved,
            this, &CGlobalControl::cookieRemoved);
    d->webProfile->cookieStore()->loadAllCookies();

    m_settings->readSettings(this);
    if (gSet->settings()->createCoredumps) {
        // create core dumps on segfaults
        rlimit rlp{};
        getrlimit(RLIMIT_CORE, &rlp);
        rlp.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlp);
    }

    m_settings->dictIndexDir = fs + QStringLiteral("dictIndex") + QDir::separator();
    QDir dictIndex(settings()->dictIndexDir);
    if (!dictIndex.exists()) {
        if (!dictIndex.mkpath(settings()->dictIndexDir)) {
            qCritical() << "Unable to create directory for dictionary indexes: " << settings()->dictIndexDir;
            m_settings->dictIndexDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        }
    }

    d->dictManager = new CGoldenDictMgr(this);

    connect(d->dictManager,&CGoldenDictMgr::showStatusBarMessage,this,[d](const QString& msg){
        if (d->activeWindow) {
            if (msg.isEmpty()) {
                d->activeWindow->statusBar()->clearMessage();
            } else {
                d->activeWindow->statusBar()->showMessage(msg);
            }
        }
    });
    connect(d->dictManager,&CGoldenDictMgr::showCriticalMessage,this,[d](const QString& msg){
        QMessageBox::critical(d->activeWindow,tr("JPReader"),msg);
    });

    d->dictNetMan = new ArticleNetworkAccessManager(this,d->dictManager);

    d->auxNetManager = new QNetworkAccessManager(this);
    d->auxNetManager->setCookieJar(new CNetworkCookieJar(d->auxNetManager));

    d->tabsListTimer.setInterval(CDefaults::tabListSavePeriod);
    connect(&d->tabsListTimer, &QTimer::timeout, settings(), &CSettings::writeTabsList);
    d->tabsListTimer.start();

    connect(this,&CGlobalControl::addAdBlockWhiteListUrl,this,[this,d](const QString& u){
        // append unblocked url to cache
        d->adblockWhiteListMutex.lock();
        d->adblockWhiteList.append(u);
        while(d->adblockWhiteList.count()>settings()->maxAdblockWhiteList)
            d->adblockWhiteList.removeFirst();
        d->adblockWhiteListMutex.unlock();
    },Qt::QueuedConnection);

    connect(this,&CGlobalControl::addNoScriptPageHost,this,[d](const QString& key, const QString& host){
        d->noScriptMutex.lock();
        d->pageScriptHosts[key].insert(host.toLower());
        d->noScriptMutex.unlock();
    },Qt::QueuedConnection);

    QTimer::singleShot(CDefaults::dictionariesLoadingDelay,d->dictManager,[this,d](){
        d->dictManager->loadDictionaries(settings()->dictPaths, settings()->dictIndexDir);
    });

    QApplication::setStyle(new CSpecMenuStyle);
    QApplication::setQuitOnLastWindowClosed(false);

    QVector<QUrl> urls;
    for (int i=1;i<QApplication::arguments().count();i++) {
        QUrl u(QApplication::arguments().at(i));
        if (!u.isEmpty() && u.isValid())
            urls << u;
    }
    addMainWindowEx(false,true,urls);
}

bool CGlobalControl::setupIPC()
{
    Q_D(CGlobalControl);
    QString serverName = QString::fromLatin1(CDefaults::IPCName);
    serverName.replace(QRegExp(QStringLiteral("[^\\w\\-. ]")), QString());

    QScopedPointer<QLocalSocket> socket(new QLocalSocket());
    socket->connectToServer(serverName);
    if (socket->waitForConnected(CDefaults::ipcTimeout)){
        // Connected. Process is already running, send message to it
        if (CGlobalControlPrivate::runnedFromQtCreator()) { // This is debug run, try to close old instance
            // Send close request
            sendIPCMessage(socket.data(),QStringLiteral("debugRestart"));
            socket->flush();
            socket->close();
            QApplication::processEvents();
            QThread::sleep(3);
            QApplication::processEvents();
            // Check for closing
            socket->connectToServer(serverName);
            if (socket->waitForConnected(CDefaults::ipcTimeout)) { // connected, unable to close
                sendIPCMessage(socket.data(),QStringLiteral("newWindow"));
                socket->flush();
                socket->close();
                return false;
            }
            // Old instance closed, start new one
            QLocalServer::removeServer(serverName);
            d->ipcServer.reset(new QLocalServer());
            d->ipcServer->listen(serverName);
            connect(d->ipcServer.data(), &QLocalServer::newConnection, this, &CGlobalControl::ipcMessageReceived);
        } else {
            sendIPCMessage(socket.data(),QStringLiteral("newWindow"));
            socket->flush();
            socket->close();
            return false;
        }
    } else {
        // New process. Startup server and gSet normally, listen for new instances
        QLocalServer::removeServer(serverName);
        d->ipcServer.reset(new QLocalServer());
        d->ipcServer->listen(serverName);
        connect(d->ipcServer.data(), &QLocalServer::newConnection, this, &CGlobalControl::ipcMessageReceived);
    }
    if (socket->isOpen())
        socket->close();
    return true;
}

void  CGlobalControl::sendIPCMessage(QLocalSocket *socket, const QString &msg)
{
    if (socket==nullptr) return;

    QString s = QStringLiteral("%1%2").arg(msg,QString::fromLatin1(CDefaults::ipcEOF));
    socket->write(s.toUtf8());
}

void CGlobalControl::cookieAdded(const QNetworkCookie &cookie)
{
    Q_D(CGlobalControl);
    if (d->auxNetManager->cookieJar())
        d->auxNetManager->cookieJar()->insertCookie(cookie);
}

void CGlobalControl::cookieRemoved(const QNetworkCookie &cookie)
{
    Q_D(CGlobalControl);
    if (d->auxNetManager->cookieJar())
        d->auxNetManager->cookieJar()->deleteCookie(cookie);
}

void CGlobalControl::atlSSLCertErrors(const QSslCertificate &cert, const QStringList &errors,
                                      const CIntList &errCodes)
{
    Q_D(CGlobalControl);
    if (d->atlCertErrorInteractive) return;

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

    d->atlCertErrorInteractive = true;
    if (mbox.exec()==QMessageBox::Yes) {
        for (const int errCode : qAsConst(errCodes)) {
            if (!m_settings->atlCerts[cert].contains(errCode))
                m_settings->atlCerts[cert].append(errCode);
        }
    }
    d->atlCertErrorInteractive = false;
}

void CGlobalControl::cleanTmpFiles()
{
    Q_D(CGlobalControl);
    for (const auto & fname : qAsConst(d->createdFiles)) {
        QFile f(fname);
        f.remove();
    }
    d->createdFiles.clear();
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

void CGlobalControl::writeSettings()
{
    m_settings->writeSettings();
}

void CGlobalControl::addFavicon(const QString &key, const QIcon &icon)
{
    Q_D(CGlobalControl);
    d->favicons[key] = icon;
}

void CGlobalControl::setTranslationEngine(CStructures::TranslationEngine engine)
{
    m_settings->translatorEngine = engine;
}

QUrl CGlobalControl::getInspectorUrl() const
{
    Q_D(const CGlobalControl);
    QUrl res;
    if (d->inspectorPort<=0 || !qEnvironmentVariableIsSet("QTWEBENGINE_REMOTE_DEBUGGING"))
        return res;

    res = QUrl::fromUserInput(QString::fromUtf8(qgetenv("QTWEBENGINE_REMOTE_DEBUGGING")));
    return res;
}

QWebEngineProfile *CGlobalControl::webProfile() const
{
    Q_D(const CGlobalControl);
    return d->webProfile;
}

QIcon CGlobalControl::appIcon() const
{
    Q_D(const CGlobalControl);
    return d->m_appIcon;
}

void CGlobalControl::setSavedAuxSaveDir(const QString &dir)
{
    m_settings->savedAuxSaveDir = dir;
}

void CGlobalControl::setSavedAuxDir(const QString &dir)
{
    m_settings->savedAuxDir = dir;
}

CMainWindow* CGlobalControl::addMainWindow()
{
    return addMainWindowEx(false, true);
}

CMainWindow* CGlobalControl::addMainWindowEx(bool withSearch, bool withViewer, const QVector<QUrl>& viewerUrls)
{
    Q_D(CGlobalControl);

    if (gSet==nullptr) return nullptr;

    auto mainWindow = new CMainWindow(withSearch,withViewer,viewerUrls);
    connect(mainWindow,&CMainWindow::aboutToClose,
            gSet,&CGlobalControl::windowDestroyed,Qt::QueuedConnection);

    d->mainWindows.append(mainWindow);

    mainWindow->show();

    mainWindow->menuTools->addAction(m_ui->actionLogNetRequests);
    mainWindow->menuTools->addSeparator();
    mainWindow->menuTools->addAction(m_ui->actionGlobalTranslator);
    mainWindow->menuTools->addAction(m_ui->actionSelectionDictionary);
    mainWindow->menuTools->addAction(m_ui->actionSnippetAutotranslate);
    mainWindow->menuTools->addSeparator();
    mainWindow->menuTools->addAction(m_ui->actionJSUsage);

    mainWindow->menuSettings->addAction(m_ui->actionAutoTranslate);
    mainWindow->menuSettings->addAction(m_ui->actionOverrideFont);
    mainWindow->menuSettings->addAction(m_ui->actionOverrideFontColor);
    mainWindow->menuSettings->addSeparator();
    mainWindow->menuSettings->addAction(m_ui->actionUseProxy);
    mainWindow->menuSettings->addAction(m_ui->actionTranslateSubSentences);

    mainWindow->menuTranslationMode->addAction(m_ui->actionTMAdditive);
    mainWindow->menuTranslationMode->addAction(m_ui->actionTMOverwriting);
    mainWindow->menuTranslationMode->addAction(m_ui->actionTMTooltip);

    m_settings->checkRestoreLoad(mainWindow);

    connect(this,&CGlobalControl::updateAllBookmarks,mainWindow,&CMainWindow::updateBookmarks);
    connect(this,&CGlobalControl::updateAllRecentLists,mainWindow,&CMainWindow::updateRecentList);
    connect(this,&CGlobalControl::updateAllCharsetLists,mainWindow,&CMainWindow::reloadCharsetList);
    connect(this,&CGlobalControl::updateAllHistoryLists,mainWindow,&CMainWindow::updateHistoryList);
    connect(this,&CGlobalControl::updateAllQueryLists,mainWindow,&CMainWindow::updateQueryHistory);
    connect(this,&CGlobalControl::updateAllRecycleBins,mainWindow,&CMainWindow::updateRecycled);
    connect(this,&CGlobalControl::updateAllLanguagesLists,mainWindow,&CMainWindow::reloadLanguagesList);

    return mainWindow;
}

void CGlobalControl::settingsDialog()
{
    m_settings->settingsDlg();
}

QRect CGlobalControl::getLastMainWindowGeometry() const
{
    Q_D(const CGlobalControl);

    QRect res;
    if (!d->mainWindows.isEmpty())
        res = d->mainWindows.constLast()->geometry();

    return res;
}

QList<QAction *> CGlobalControl::getTranslationLanguagesActions() const
{
    return m_ui->languageSelector->actions();
}

bool CGlobalControl::isBlockTabCloseActive() const
{
    Q_D(const CGlobalControl);
    return d->blockTabCloseActive;
}

const CSettings* CGlobalControl::settings() const
{
    return m_settings.data();
}

const CGlobalUI *CGlobalControl::ui() const
{
    return m_ui.data();
}

CMainWindow *CGlobalControl::activeWindow() const
{
    Q_D(const CGlobalControl);
    return d->activeWindow;
}

BookmarksManager *CGlobalControl::bookmarksManager() const
{
    Q_D(const CGlobalControl);
    return d->bookmarksManager;
}

CDownloadManager *CGlobalControl::downloadManager() const
{
    Q_D(const CGlobalControl);
    return d->downloadManager.data();
}

CLogDisplay *CGlobalControl::logWindow() const
{
    Q_D(const CGlobalControl);
    return d->logWindow.data();
}

ArticleNetworkAccessManager *CGlobalControl::dictionaryNetworkAccessManager() const
{
    Q_D(const CGlobalControl);
    return d->dictNetMan;
}

QNetworkAccessManager *CGlobalControl::auxNetworkAccessManager() const
{
    Q_D(const CGlobalControl);
    return d->auxNetManager;
}

const QHash<QString, QIcon> &CGlobalControl::favicons() const
{
    Q_D(const CGlobalControl);
    return d->favicons;
}

CGoldenDictMgr *CGlobalControl::dictionaryManager() const
{
    Q_D(const CGlobalControl);
    return d->dictManager;
}

const CUrlHolderVector &CGlobalControl::recycleBin() const
{
    Q_D(const CGlobalControl);
    return d->recycleBin;
}

const CUrlHolderVector &CGlobalControl::mainHistory() const
{
    Q_D(const CGlobalControl);
    return d->mainHistory;
}

const QStringList &CGlobalControl::recentFiles() const
{
    Q_D(const CGlobalControl);
    return d->recentFiles;
}

const QStringList &CGlobalControl::searchHistory() const
{
    Q_D(const CGlobalControl);
    return d->searchHistory;
}

void CGlobalControl::blockTabClose()
{
    Q_D(CGlobalControl);
    d->blockTabCloseActive=true;
    QTimer::singleShot(CDefaults::tabCloseInterlockDelay,this,[d](){
        d->blockTabCloseActive=false;
    });
}

void CGlobalControl::showDictionaryWindow()
{
    showDictionaryWindowEx(QString());
}

void CGlobalControl::showDictionaryWindowEx(const QString &text)
{
    Q_D(CGlobalControl);
    if (d->auxDictionary.isNull()) {
        d->auxDictionary.reset(new CAuxDictionary());
        d->auxDictionary->show();
        d->auxDictionary->adjustSplitters();
    }

    if (!text.isEmpty()) {
        d->auxDictionary->findWord(text);
    } else {
        d->auxDictionary->restoreWindow();
    }
}

void CGlobalControl::appendSearchHistory(const QStringList& req)
{
    Q_D(CGlobalControl);
    for(int i=0;i<req.count();i++) {
        int idx = d->searchHistory.indexOf(req.at(i));
        if (idx>=0)
            d->searchHistory.removeAt(idx);
        d->searchHistory.insert(0,req.at(i));
    }
}

void CGlobalControl::appendRecycled(const QString& title, const QUrl& url)
{
    Q_D(CGlobalControl);
    int idx = d->recycleBin.indexOf(CUrlHolder(title,url));
    if (idx>=0) {
        d->recycleBin.move(idx,0);
    } else {
        d->recycleBin.prepend(CUrlHolder(title,url));
    }

    if (d->recycleBin.count()>settings()->maxRecycled)
        d->recycleBin.removeLast();

    Q_EMIT updateAllRecycleBins();
}

void CGlobalControl::removeRecycledItem(int idx)
{
    Q_D(CGlobalControl);
    d->recycleBin.removeAt(idx);
    Q_EMIT updateAllRecycleBins();
}

void CGlobalControl::appendMainHistory(const CUrlHolder &item)
{
    Q_D(CGlobalControl);
    if (item.url.toString().startsWith(QStringLiteral("data:"),Qt::CaseInsensitive)) return;

    if (d->mainHistory.contains(item))
        d->mainHistory.removeOne(item);
    d->mainHistory.prepend(item);

    while (d->mainHistory.count()>settings()->maxHistory)
        d->mainHistory.removeLast();

    Q_EMIT updateAllHistoryLists();
}

bool CGlobalControl::updateMainHistoryTitle(const CUrlHolder &item, const QString& newTitle)
{
    Q_D(CGlobalControl);
    if (d->mainHistory.contains(item)) {
        int idx = d->mainHistory.indexOf(item);
        if (idx>=0) {
            d->mainHistory[idx].title = newTitle;
            Q_EMIT updateAllHistoryLists();
            return true;
        }
    }
    return false;
}

void CGlobalControl::appendRecent(const QString& filename)
{
    Q_D(CGlobalControl);
    QFileInfo fi(filename);
    if (!fi.exists()) return;

    if (d->recentFiles.contains(filename))
        d->recentFiles.removeAll(filename);
    d->recentFiles.prepend(filename);

    while (d->recentFiles.count()>settings()->maxRecent)
        d->recentFiles.removeLast();

    Q_EMIT updateAllRecentLists();
}

void CGlobalControl::appendCreatedFiles(const QString &filename)
{
    Q_D(CGlobalControl);
    d->createdFiles.append(filename);
}

void CGlobalControl::ipcMessageReceived()
{
    Q_D(CGlobalControl);
    QLocalSocket *socket = d->ipcServer->nextPendingConnection();
    QByteArray bmsg;
    do {
        if (!socket->waitForReadyRead(CDefaults::ipcTimeout)) return;
        bmsg.append(socket->readAll());
    } while (!bmsg.contains(CDefaults::ipcEOF));
    socket->close();
    socket->deleteLater();

    QStringList cmd = QString::fromUtf8(bmsg).split('\n');
    if (cmd.first().startsWith(QStringLiteral("newWindow"))) {
        addMainWindow();
    } else if (cmd.first().startsWith(QStringLiteral("debugRestart"))) {
        qInfo() << tr("Closing jpreader instance (pid: %1)"
                      "after debugRestart request")
                   .arg(QApplication::applicationPid());
        cleanupAndExit();
    }
}

void CGlobalControl::focusChanged(QWidget *old, QWidget *now)
{
    Q_UNUSED(old)
    Q_D(CGlobalControl);
    if (now==nullptr) return;
    auto mw = qobject_cast<CMainWindow*>(now->window());
    if (mw==nullptr) return;
    d->activeWindow=mw;
}

void CGlobalControl::windowDestroyed(CMainWindow *obj)
{
    Q_D(CGlobalControl);
    if (obj==nullptr) return;
    d->mainWindows.removeOne(obj);
    if (d->activeWindow==obj) {
        if (d->mainWindows.count()>0) {
            d->activeWindow = d->mainWindows.constFirst();
            d->activeWindow->activateWindow();
        } else {
            d->activeWindow=nullptr;
            cleanupAndExit();
        }
    }
}

void CGlobalControl::cleanupAndExit()
{
    Q_D(CGlobalControl);
    if (d->cleaningState) return;
    d->cleaningState = true;

    m_settings->clearTabsList();
    m_settings->writeSettings();
    cleanTmpFiles();

    Q_EMIT stopTranslators();

    if (d->mainWindows.count()>0) {
        for (CMainWindow* w : qAsConst(d->mainWindows)) {
            if (w)
                w->close();
        }
    }
    m_ui->gctxTranHotkey.unsetShortcut();
    CPDFWorker::freePdfToText();

    d->ipcServer->close();

    QMetaObject::invokeMethod(QApplication::instance(),&QApplication::quit,Qt::QueuedConnection);
}

bool CGlobalControl::isUrlBlocked(const QUrl& url)
{
    QString dummy = QString();
    return isUrlBlocked(url,dummy);
}

bool CGlobalControl::isUrlBlocked(const QUrl& url, QString &filter)
{
    Q_D(const CGlobalControl);
    if (!settings()->useAdblock) return false;
    if (!url.scheme().startsWith(QStringLiteral("http"),Qt::CaseInsensitive)) return false;

    filter.clear();
    QString u = url.toString(CSettings::adblockUrlFmt);

    if (d->adblockWhiteList.contains(u)) return false;

    for (const CAdBlockRule& rule : qAsConst(d->adblock)) {
        if (rule.networkMatch(u)) {
            filter = rule.filter();
            return true;
        }
    }

    Q_EMIT addAdBlockWhiteListUrl(u);

    return false;
}

bool CGlobalControl::isScriptBlocked(const QUrl &url, const QUrl& origin)
{
    Q_D(const CGlobalControl);
    if (!settings()->useNoScript) return false;
    if (url.isRelative() || url.isLocalFile()) return false;

    const QString host = url.host();
    const QString key = origin.toString(CSettings::adblockUrlFmt);

    Q_EMIT addNoScriptPageHost(key, host);

    return !(d->noScriptWhiteList.contains(host));
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
    Q_D(CGlobalControl);

    d->adblock.append(urls);

    d->adblockWhiteListMutex.lock();
    d->adblockWhiteList.clear();
    d->adblockWhiteListMutex.unlock();
}

void CGlobalControl::clearNoScriptPageHosts(const QString &origin)
{
    Q_D(CGlobalControl);
    d->noScriptMutex.lock();
    d->pageScriptHosts[origin].clear();
    d->noScriptMutex.unlock();
}

CStringSet CGlobalControl::getNoScriptPageHosts(const QString &origin)
{
    Q_D(CGlobalControl);
    CStringSet res;
    d->noScriptMutex.lock();
    res = d->pageScriptHosts.value(origin);
    d->noScriptMutex.unlock();
    return res;
}

void CGlobalControl::removeNoScriptWhitelistHost(const QString &host)
{
    Q_D(CGlobalControl);
    d->noScriptMutex.lock();
    d->noScriptWhiteList.remove(host);
    d->noScriptMutex.unlock();
}

void CGlobalControl::addNoScriptWhitelistHost(const QString &host)
{
    Q_D(CGlobalControl);
    d->noScriptMutex.lock();
    d->noScriptWhiteList.insert(host);
    d->noScriptMutex.unlock();
}

bool CGlobalControl::containsNoScriptWhitelist(const QString &host) const
{
    Q_D(const CGlobalControl);
    return d->noScriptWhiteList.contains(host);
}

bool CGlobalControl::haveSavedPassword(const QUrl &origin)
{
    QString user;
    QString pass;
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

QVector<CUserScript> CGlobalControl::getUserScriptsForUrl(const QUrl &url, bool isMainFrame, bool isContextMenu)
{
    Q_D(CGlobalControl);

    d->userScriptsMutex.lock();

    QVector<CUserScript> scripts;

    for (auto it = d->userScripts.constBegin(), end = d->userScripts.constEnd(); it != end; ++it) {
        if (it.value().isEnabledForUrl(url) &&
                (isMainFrame || it.value().shouldRunOnSubFrames()) &&
                ((isContextMenu && it.value().shouldRunFromContextMenu()) ||
                 (!isContextMenu && !it.value().shouldRunFromContextMenu()))) {

            scripts.append(it.value());
        }
    }

    d->userScriptsMutex.unlock();

    return scripts;
}

void CGlobalControl::initUserScripts(const CStringHash &scripts)
{
    Q_D(CGlobalControl);

    d->userScriptsMutex.lock();

    d->userScripts.clear();
    for (auto it = scripts.constBegin(), end = scripts.constEnd(); it != end; ++it)
        d->userScripts[it.key()] = CUserScript(it.key(), it.value());

    d->userScriptsMutex.unlock();
}

CStringHash CGlobalControl::getUserScripts()
{
    Q_D(CGlobalControl);

    d->userScriptsMutex.lock();

    CStringHash res;

    for (auto it = d->userScripts.constBegin(), end = d->userScripts.constEnd(); it != end; ++it)
        res[it.key()] = (*it).getSource();

    d->userScriptsMutex.unlock();

    return res;
}

void CGlobalControl::initLanguagesList()
{
    Q_D(CGlobalControl);
    const QList<QLocale> allLocales = QLocale::matchingLocales(
                                          QLocale::AnyLanguage,
                                          QLocale::AnyScript,
                                          QLocale::AnyCountry);

    d->langNamesList.clear();
    d->langSortedBCP47List.clear();

    for(const QLocale &locale : allLocales) {
        QString bcp = locale.bcp47Name();
        QString name = QStringLiteral("%1 (%2)")
                       .arg(QLocale::languageToString(locale.language()),bcp);

        // filter out unsupported codes for dialects
        if (bcp.contains('-') && !bcp.startsWith(QStringLiteral("zh"))) continue;

        // replace C locale with English
        if (bcp == QStringLiteral("en"))
            name = QStringLiteral("English (en)");

        if (!d->langNamesList.contains(bcp)) {
            d->langNamesList.insert(bcp,name);
            d->langSortedBCP47List.insert(name,bcp);
        }
    }
}

QString CGlobalControl::getLanguageName(const QString& bcp47Name) const
{
    Q_D(const CGlobalControl);
    return d->langNamesList.value(bcp47Name);
}

QStringList CGlobalControl::getLanguageCodes() const
{
    Q_D(const CGlobalControl);
    return d->langSortedBCP47List.values();
}

void CGlobalControl::showLightTranslator(const QString &text)
{
    Q_D(CGlobalControl);
    if (d->lightTranslator.isNull())
        d->lightTranslator.reset(new CLightTranslator());

    d->lightTranslator->restoreWindow();

    if (!text.isEmpty())
        d->lightTranslator->appendSourceText(text);
}

void CGlobalControl::updateProxy(bool useProxy)
{
    updateProxyWithMenuUpdate(useProxy, false);
}

void CGlobalControl::updateProxyWithMenuUpdate(bool useProxy, bool forceMenuUpdate)
{
    m_settings->proxyUse = useProxy;

    QNetworkProxy proxy = QNetworkProxy();

    if (settings()->proxyUse && !settings()->proxyHost.isEmpty()) {
        proxy = QNetworkProxy(settings()->proxyType,settings()->proxyHost,
                              settings()->proxyPort,settings()->proxyLogin,
                              settings()->proxyPassword);
    }

    QNetworkProxy::setApplicationProxy(proxy);

    if (forceMenuUpdate)
        m_ui->actionUseProxy->setChecked(settings()->proxyUse);
}

void CGlobalControl::clearCaches()
{
    Q_D(CGlobalControl);
    d->webProfile->clearHttpCache();
}

void CGlobalControl::forceCharset()
{
    const int maxCharsetHistory = 10;

    auto act = qobject_cast<QAction*>(sender());
    if (act==nullptr) return;

    QString cs = act->data().toString();
    if (!cs.isEmpty()) {
        QTextCodec* codec = QTextCodec::codecForName(cs.toLatin1().data());
        if (codec!=nullptr)
            cs = QString::fromUtf8(codec->name());

        m_settings->charsetHistory.removeAll(cs);
        m_settings->charsetHistory.prepend(cs);
        if (m_settings->charsetHistory.count()>maxCharsetHistory)
            m_settings->charsetHistory.removeLast();
    }

    m_settings->forcedCharset = cs;
    Q_EMIT updateAllCharsetLists();

    if (webProfile()!=nullptr && webProfile()->settings()!=nullptr) {
        webProfile()->settings()->setDefaultTextEncoding(cs);
    }

}

QUrl CGlobalControl::createSearchUrl(const QString& text, const QString& engine) const
{
    Q_D(const CGlobalControl);
    if (d->ctxSearchEngines.isEmpty())
        return QUrl(QStringLiteral("about://blank"));

    auto it = d->ctxSearchEngines.constKeyValueBegin();
    QString url = (*it).second;
    if (engine.isEmpty() && !settings()->defaultSearchEngine.isEmpty())
        url = d->ctxSearchEngines.value(settings()->defaultSearchEngine);
    if (!engine.isEmpty() && d->ctxSearchEngines.contains(engine))
        url = d->ctxSearchEngines.value(engine);

    url.replace(QStringLiteral("%s"),text);
    url.replace(QStringLiteral("%ps"),QString::fromUtf8(QUrl::toPercentEncoding(text)));

    return QUrl::fromUserInput(url);
}

QStringList CGlobalControl::getSearchEngines() const
{
    Q_D(const CGlobalControl);
    return d->ctxSearchEngines.keys();
}
