#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineUrlScheme>
#include <QNetworkReply>
#include <QAuthenticator>
#include <QElapsedTimer>

#include <algorithm>
#include <execution>

extern "C" {
#include <sys/resource.h>
}

#include "startup.h"
#include "control.h"
#include "control_p.h"
#include "contentfiltering.h"
#include "network.h"
#include "ui.h"
#include "browser-utils/adblockrule.h"
#include "browser-utils/bookmarks.h"
#include "browser-utils/browsercontroller.h"
#include "browser-utils/downloadmanager.h"
#include "translator/translatorcache.h"
#include "translator/lighttranslator.h"
#include "utils/genericfuncs.h"
#include "utils/pdftotext.h"
#include "utils/logdisplay.h"
#include "utils/workermonitor.h"

#include "auxtranslator_adaptor.h"
#include "browsercontroller_adaptor.h"

namespace CDefaults {
const auto ipcEOF = "\n###";
const int tabListSavePeriod = 30000;
const int dictionariesLoadingDelay = 1500;
const int ipcTimeout = 1000;
const int workerMaxShutdownTimeMS = 5000;
const int workerWaitGranularityMS = 250;
}

CGlobalStartup::CGlobalStartup(CGlobalControl *parent)
    : QObject(parent),
      m_g(parent)
{
}

CGlobalStartup::~CGlobalStartup() = default;

void CGlobalStartup::initialize()
{
    const bool cliMode = m_g->m_actions.isNull();

    QElapsedTimer initTime;
    initTime.start();

    QCoreApplication::setOrganizationName(QSL("kernel1024"));
    QCoreApplication::setApplicationName(QSL("jpreader"));
    if (!cliMode)
        QGuiApplication::setApplicationDisplayName(QSL("JPReader"));

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
    qRegisterMetaTypeStreamOperators<CSubsentencesMode>("CSubsentencesMode");
    qRegisterMetaTypeStreamOperators<CTranslatorStatistics>("CTranslatorStatistics");
    qRegisterMetaTypeStreamOperators<CSelectedLangPairs>("CSelectedLangPairs");

    initLanguagesList();

    CPDFWorker::initPdfToText();

    if (cliMode) {
        m_g->d_func()->cliWorker.reset(new CCLIWorker());
        connect(m_g->d_func()->cliWorker.data(),&CCLIWorker::finished,this,&CGlobalStartup::cleanupAndExit);

    } else {
        if (!setupIPC())
            ::exit(0);

        m_g->d_func()->uiTranslator.reset(new CUITranslator());
        QCoreApplication::installTranslator(m_g->d_func()->uiTranslator.data());

        m_g->d_func()->logWindow.reset(new CLogDisplay());
        m_g->d_func()->zipWriter = new CZipWriter(this);
        m_g->d_func()->downloadManager.reset(new CDownloadManager(nullptr,m_g->d_func()->zipWriter));
        m_g->d_func()->bookmarksManager = new BookmarksManager(this);
        m_g->d_func()->workerMonitor.reset(new CWorkerMonitor());

        m_g->d_func()->auxTranslatorDBus = new CAuxTranslator(this);
        m_g->d_func()->browserControllerDBus = new CBrowserController(this);
        new AuxtranslatorAdaptor(m_g->d_func()->auxTranslatorDBus);
        new BrowsercontrollerAdaptor(m_g->d_func()->browserControllerDBus);
        QDBusConnection dbus = QDBusConnection::sessionBus();
        if (!dbus.registerObject(QSL("/auxTranslator"),m_g->d_func()->auxTranslatorDBus))
            qCritical() << dbus.lastError().name() << dbus.lastError().message();
        if (!dbus.registerObject(QSL("/browserController"),m_g->d_func()->browserControllerDBus))
            qCritical() << dbus.lastError().name() << dbus.lastError().message();
        if (!dbus.registerService(QString::fromLatin1(CDefaults::DBusName)))
            qCritical() << dbus.lastError().name() << dbus.lastError().message();

        connect(m_g->app(m_g->parent()), &QApplication::focusChanged, m_g->ui(), &CGlobalUI::focusChanged);

        connect(m_g->m_actions->actionUseProxy, &QAction::toggled, m_g->net(), &CGlobalNetwork::updateProxy);

        connect(m_g->m_actions->actionJSUsage,&QAction::toggled,m_g,[](bool checked){
            gSet->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,checked);
        });
        connect(m_g->m_actions->actionLogNetRequests,&QAction::toggled,m_g,[](bool checked){
            gSet->m_settings->debugNetReqLogging = checked;
        });

        m_g->d_func()->webProfile = new QWebEngineProfile(QSL("jpreader"),this);

        QString fs = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

        if (fs.isEmpty()) fs = QDir::homePath() + QDir::separator() + QSL(".cache")
                               + QDir::separator() + QSL("jpreader");
        if (!fs.endsWith(QDir::separator())) fs += QDir::separator();

        QString fcache = fs + QSL("cache") + QDir::separator();
        m_g->d_func()->webProfile->setCachePath(fcache);
        QString fdata = fs + QSL("local_storage") + QDir::separator();
        m_g->d_func()->webProfile->setPersistentStoragePath(fdata);

        m_g->d_func()->webProfile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        m_g->d_func()->webProfile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

        m_g->d_func()->webProfile->setSpellCheckEnabled(false);

        m_g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled,true);
        m_g->d_func()->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,true);

        connect(m_g->d_func()->webProfile, &QWebEngineProfile::downloadRequested,
                m_g->d_func()->downloadManager.data(), &CDownloadManager::handleDownload);
        m_g->d_func()->webProfile->setUrlRequestInterceptor(new CSpecUrlInterceptor(this));
        connect(m_g->d_func()->webProfile->cookieStore(), &QWebEngineCookieStore::cookieAdded,
                m_g->net(), &CGlobalNetwork::cookieAdded);
        connect(m_g->d_func()->webProfile->cookieStore(), &QWebEngineCookieStore::cookieRemoved,
                m_g->net(), &CGlobalNetwork::cookieRemoved);
        m_g->d_func()->webProfile->cookieStore()->loadAllCookies();

        m_g->d_func()->webProfile->installUrlSchemeHandler(CMagicFileSchemeHandler::getScheme().toUtf8(),
                                                           new CMagicFileSchemeHandler(this));

        m_g->d_func()->translatorCache = new CTranslatorCache(this);
        QString tcache = fs + QSL("translator_cache") + QDir::separator();
        m_g->d_func()->translatorCache->setCachePath(tcache);
    }

    m_g->m_settings->readSettings(m_g);
    if (m_g->m_settings->createCoredumps) {
        // create core dumps on segfaults
        rlimit rlp{};
        getrlimit(RLIMIT_CORE, &rlp);
        rlp.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlp);
    }

    if (!cliMode) {
        m_g->d_func()->dictManager = new ZDict::ZDictController(this);
        m_g->d_func()->auxNetManager = new QNetworkAccessManager(this);
        m_g->d_func()->auxNetManager->setCookieJar(new CNetworkCookieJar(m_g->d_func()->auxNetManager));
        connect(m_g->d_func()->auxNetManager,&QNetworkAccessManager::authenticationRequired,
                m_g->net(),&CGlobalNetwork::authenticationRequired);
        connect(m_g->d_func()->auxNetManager,&QNetworkAccessManager::proxyAuthenticationRequired,
                m_g->net(),&CGlobalNetwork::proxyAuthenticationRequired);
        connect(m_g->d_func()->auxNetManager,&QNetworkAccessManager::sslErrors,
                m_g->net(),&CGlobalNetwork::auxSSLCertError);

        m_g->d_func()->tabsListTimer.setInterval(CDefaults::tabListSavePeriod);
        connect(&(m_g->d_func()->tabsListTimer), &QTimer::timeout, m_g->settings(), &CSettings::writeTabsList);
        m_g->d_func()->tabsListTimer.start();

        connect(m_g->contentFilter(),&CGlobalContentFiltering::addAdBlockWhiteListUrl,this,
                [](const QString& u){
            // append unblocked url to cache
            gSet->d_func()->adblockWhiteListMutex.lock();
            gSet->d_func()->adblockWhiteList.append(u);
            while (gSet->d_func()->adblockWhiteList.count() > gSet->m_settings->maxAdblockWhiteList)
                gSet->d_func()->adblockWhiteList.removeFirst();
            gSet->d_func()->adblockWhiteListMutex.unlock();
        },Qt::QueuedConnection);

        connect(m_g->contentFilter(),&CGlobalContentFiltering::addNoScriptPageHost,this,
                [](const QString& key, const QString& host){
            gSet->d_func()->noScriptMutex.lock();
            gSet->d_func()->pageScriptHosts[key].insert(host.toLower());
            gSet->d_func()->noScriptMutex.unlock();
        },Qt::QueuedConnection);

        QTimer::singleShot(CDefaults::dictionariesLoadingDelay,m_g->d_func()->dictManager,[](){
            gSet->d_func()->dictManager->loadDictionaries(gSet->m_settings->dictPaths);
        });

        QApplication::setStyle(new CSpecMenuStyle);
        QApplication::setQuitOnLastWindowClosed(false);

        QVector<QUrl> openUrls;
        for (int i=1;i<QCoreApplication::arguments().count();i++) {
            QUrl u(QCoreApplication::arguments().at(i));
            if (!u.isEmpty() && u.isValid())
                openUrls << u;
        }

        m_g->m_ui->addMainWindowEx(false,true,openUrls);

        qInfo() << "Initialization time, ms: " << initTime.elapsed();

    } else {
        if (!m_g->d_func()->cliWorker->parseArguments())
            ::exit(1);

        QMetaObject::invokeMethod(m_g->d_func()->cliWorker.data(),&CCLIWorker::start,Qt::QueuedConnection);
    }
}

void CGlobalStartup::preinit(int &argc, char *argv[], bool *cliMode)
{
    QWebEngineUrlScheme scheme(CMagicFileSchemeHandler::getScheme().toUtf8());
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    scheme.setFlags(QWebEngineUrlScheme::LocalScheme | QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);

    *cliMode = (argc > 1);
    for (int i = 1; i < argc; ++i) {
        const QString param = QString::fromUtf8(argv[i]);
        if (param.startsWith(QSL("-"))) {
            *cliMode = true;
        } else {
            QUrl u(param);
            if (!u.scheme().isEmpty() || u.isLocalFile()) {
                *cliMode = false;
                break;
            }
        }
    }
}

bool CGlobalStartup::setupIPC()
{
    if (!m_g->d_func()->ipcServer.isNull()) return false;

    QString serverName = QString::fromLatin1(CDefaults::IPCName);
    serverName.replace(QRegularExpression(QSL("[^\\w\\-.]")), QString());

    QScopedPointer<QLocalSocket> socket(new QLocalSocket());
    socket->connectToServer(serverName);
    if (socket->waitForConnected(CDefaults::ipcTimeout)){
        // Connected. Process is already running, send message to it
        if (CGlobalControlPrivate::runnedFromQtCreator()) { // This is debug run, try to close old instance
            // Send close request
            sendIPCMessage(socket.data(),QSL("debugRestart"));
            socket->flush();
            socket->close();
            QApplication::processEvents();
            QThread::sleep(3);
            QApplication::processEvents();
            // Check for closing
            socket->connectToServer(serverName);
            if (socket->waitForConnected(CDefaults::ipcTimeout)) { // connected, unable to close
                sendIPCMessage(socket.data(),QSL("newWindow"));
                socket->flush();
                socket->close();
                return false;
            }
            // Old instance closed, start new one
            QLocalServer::removeServer(serverName);
            m_g->d_func()->ipcServer.reset(new QLocalServer());
            m_g->d_func()->ipcServer->listen(serverName);
            connect(m_g->d_func()->ipcServer.data(), &QLocalServer::newConnection,
                    this, &CGlobalStartup::ipcMessageReceived);
        } else {
            sendIPCMessage(socket.data(),QSL("newWindow"));
            socket->flush();
            socket->close();
            return false;
        }
    } else {
        // New process. Startup server and m_g normally, listen for new instances
        QLocalServer::removeServer(serverName);
        m_g->d_func()->ipcServer.reset(new QLocalServer());
        m_g->d_func()->ipcServer->listen(serverName);
        connect(m_g->d_func()->ipcServer.data(), &QLocalServer::newConnection,
                this, &CGlobalStartup::ipcMessageReceived);
    }
    if (socket->isOpen())
        socket->close();
    return true;
}

void  CGlobalStartup::sendIPCMessage(QLocalSocket *socket, const QString &msg)
{
    if (socket==nullptr) return;

    QString s = QSL("%1%2").arg(msg,QString::fromLatin1(CDefaults::ipcEOF));
    socket->write(s.toUtf8());
}

void CGlobalStartup::ipcMessageReceived()
{
    QLocalSocket *socket = m_g->d_func()->ipcServer->nextPendingConnection();
    QByteArray bmsg;
    do {
        if (!socket->waitForReadyRead(CDefaults::ipcTimeout)) return;
        bmsg.append(socket->readAll());
    } while (!bmsg.contains(CDefaults::ipcEOF));
    socket->close();
    socket->deleteLater();

    QStringList cmd = QString::fromUtf8(bmsg).split('\n');
    if (cmd.first().startsWith(QSL("newWindow"))) {
        m_g->m_ui->addMainWindowEx(false, true);
    } else if (cmd.first().startsWith(QSL("debugRestart"))) {
        qInfo() << tr("Closing jpreader instance (pid: %1)"
                      "after debugRestart request")
                   .arg(QApplication::applicationPid());
        cleanupAndExit();
    }
}

void CGlobalStartup::stopAndCloseWorkers()
{
    QElapsedTimer tmr;

    Q_EMIT stopWorkers();
    m_g->d_func()->zipWriter->abortAllWorkers();

    tmr.start();
    while ((!m_g->d_func()->workerPool.isEmpty() || CZipWriter::isBusy()) &&
           (tmr.elapsed() < CDefaults::workerMaxShutdownTimeMS)) {
        CGenericFuncs::processedMSleep(CDefaults::workerWaitGranularityMS);
    }
    CGenericFuncs::processedMSleep(CDefaults::workerWaitGranularityMS);

    if (CZipWriter::isBusy())
        m_g->d_func()->zipWriter->terminateAllWorkers();
    if (!m_g->d_func()->workerPool.isEmpty()) {
        Q_EMIT terminateWorkers();
        tmr.start();
        while (!m_g->d_func()->workerPool.isEmpty() &&
               tmr.elapsed() < CDefaults::workerMaxShutdownTimeMS) {
            CGenericFuncs::processedMSleep(CDefaults::workerWaitGranularityMS);
        }
        CGenericFuncs::processedMSleep(CDefaults::workerWaitGranularityMS);
        m_g->d_func()->workerPool.clear(); // Just stop problematic workers, do not try to free memory correctly
    }
}

void CGlobalStartup::cleanupAndExit()
{
    if (m_g->d_func()->cleaningState) return;
    m_g->d_func()->cleaningState = true;

    if (!m_g->m_actions.isNull()) {
        m_g->m_settings->clearTabsList();
        m_g->m_settings->writeSettings();

        if (m_g->d_func()->mainWindows.count()>0) {
            for (CMainWindow* w : qAsConst(m_g->d_func()->mainWindows)) {
                if (w)
                    w->close();
            }
        }

        m_g->m_actions->gctxTranHotkey.unsetShortcut();
    }
    cleanTmpFiles();

    stopAndCloseWorkers();

    CPDFWorker::freePdfToText();

    if (!m_g->m_actions.isNull()) {
        // free global objects
        m_g->d_func()->logWindow.reset(nullptr);
        m_g->d_func()->downloadManager.reset(nullptr);
        m_g->d_func()->workerMonitor.reset(nullptr);
        m_g->d_func()->auxDictionary.reset(nullptr);
        m_g->d_func()->lightTranslator.reset(nullptr);
        m_g->d_func()->ipcServer->close();
        m_g->d_func()->ipcServer.reset(nullptr);
    }

    QMetaObject::invokeMethod(QApplication::instance(),&QApplication::quit,Qt::QueuedConnection);
}


bool CGlobalStartup::setupThreadedWorker(CAbstractThreadWorker *worker)
{
    if (m_g->d_func()->cleaningState) return false;

    if (worker == nullptr) return false;

    m_g->d_func()->workerPool.append(worker);

    auto *thread = new QThread();
    worker->moveToThread(thread);

    connect(worker,&CAbstractThreadWorker::finished,this,&CGlobalStartup::cleanupWorker,Qt::QueuedConnection);
    connect(worker,&CAbstractThreadWorker::finished,thread,&QThread::quit);
    connect(thread,&QThread::finished,worker,&CAbstractThreadWorker::deleteLater);
    connect(thread,&QThread::finished,thread,&QThread::deleteLater);
    connect(this,&CGlobalStartup::stopWorkers,
            worker,&CAbstractThreadWorker::abort,Qt::QueuedConnection);
    connect(this,&CGlobalStartup::terminateWorkers,thread,&QThread::terminate);

    connect(worker,&CAbstractThreadWorker::started,
            m_g->actions(),&CGlobalActions::updateBusyCursor,Qt::QueuedConnection);

    m_g->d_func()->workerMonitor->workerStarted(worker);

    thread->setObjectName(QSL("WRK-%1").arg(QString::fromLatin1(worker->metaObject()->className())));
    thread->start();

    return true;
}

bool CGlobalStartup::isThreadedWorkersActive() const
{
    return (!(m_g->d_func()->workerPool.isEmpty()));
}

void CGlobalStartup::cleanupWorker()
{
    auto *worker = qobject_cast<CAbstractThreadWorker *>(sender());
    if (worker) {
        m_g->d_func()->workerMonitor->workerAboutToFinished(worker);
        m_g->d_func()->workerPool.removeAll(worker);
    }

    m_g->m_actions->updateBusyCursor();
}

void CGlobalStartup::cleanTmpFiles()
{
    std::for_each(std::execution::par,m_g->d_func()->createdFiles.constBegin(),m_g->d_func()->createdFiles.constEnd(),
                  [](const QString& fname){
        QFile f(fname);
        f.remove();
    });
    m_g->d_func()->createdFiles.clear();
}

void CGlobalStartup::initLanguagesList()
{
    const QList<QLocale> allLocales = QLocale::matchingLocales(
                                          QLocale::AnyLanguage,
                                          QLocale::AnyScript,
                                          QLocale::AnyCountry);

    m_g->d_func()->langNamesList.clear();
    m_g->d_func()->langSortedBCP47List.clear();

    for(const QLocale &locale : allLocales) {
        QString bcp = locale.bcp47Name();
        QString name = QSL("%1 (%2)")
                       .arg(QLocale::languageToString(locale.language()),bcp);

        // filter out unsupported codes for dialects
        if (bcp.contains('-') && !bcp.startsWith(QSL("zh"))) continue;

        // replace C locale with English
        if (bcp == QSL("en"))
            name = QSL("English (en)");

        if (!m_g->d_func()->langNamesList.contains(bcp)) {
            m_g->d_func()->langNamesList.insert(bcp,name);
            m_g->d_func()->langSortedBCP47List.insert(name,bcp);
        }
    }
}
