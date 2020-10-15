#include <QNetworkProxy>
#include <QMessageBox>
#include <QLocalSocket>
#include <QWebEngineSettings>
#include <QNetworkCookieJar>
#include <QStandardPaths>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QWebEngineUrlScheme>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QPointer>
#include <QAtomicInteger>
#include <algorithm>
#include <execution>

extern "C" {
#include <sys/resource.h>
#include <unicode/uclean.h>
}

#include "mainwindow.h"
#include "settingstab.h"
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
#include "downloadwriter.h"
#include "translatorcache.h"
#include "translatorstatisticstab.h"
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
const int translatorMaxShutdownTimeMS = 5000;
const int translatorWaitGranularityMS = 250;
}

CGlobalControl::CGlobalControl(QCoreApplication *parent) :
    QObject(parent),
    dptr(new CGlobalControlPrivate(this)),
    m_settings(new CSettings(this))
{
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        m_ui.reset(new CGlobalUI(this));
    }
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
    static QPointer<CGlobalControl> inst;
    static QAtomicInteger<bool> initializedOnce(false);

    if (inst.isNull()) {
        if (initializedOnce.testAndSetAcquire(false,true)) {
            inst = new CGlobalControl(QCoreApplication::instance());
            return inst.data();
        }

        qCritical() << "Accessing to gSet after destruction!!!";
        return nullptr;
    }

    return inst.data();
}

QApplication *CGlobalControl::app(QObject *parentApp)
{
    QObject* capp = parentApp;
    if (capp==nullptr)
        capp = QCoreApplication::instance();

    auto *res = qobject_cast<QApplication *>(capp);
    return res;
}

void CGlobalControl::initialize()
{
    Q_D(CGlobalControl);

    const bool cliMode = m_ui.isNull();

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
        d->cliWorker.reset(new CCLIWorker());
        connect(d->cliWorker.data(),&CCLIWorker::finished,this,&CGlobalControl::cleanupAndExit);

    } else {
        if (!setupIPC())
            ::exit(0);

        d->uiTranslator.reset(new CUITranslator());
        QCoreApplication::installTranslator(d->uiTranslator.data());

        d->downloadWriter = new CDownloadWriter();
        addWorkerToPool(d->downloadWriter);
        auto* th = new QThread();
        connect(d->downloadWriter,&CDownloadWriter::finished,th,&QThread::quit);
        connect(d->downloadWriter,&CDownloadWriter::finished,
                this,&CGlobalControl::cleanupWorker,Qt::QueuedConnection);
        connect(th,&QThread::finished,d->downloadWriter,&CDownloadWriter::deleteLater);
        connect(th,&QThread::finished,th,&QThread::deleteLater);
        connect(this,&CGlobalControl::stopWorkers,d->downloadWriter,
                &CDownloadWriter::terminateStop,Qt::QueuedConnection);
        connect(this,&CGlobalControl::terminateWorkers,
                th,&QThread::terminate);
        d->downloadWriter->moveToThread(th);
        th->start();

        d->logWindow.reset(new CLogDisplay());
        d->downloadManager.reset(new CDownloadManager());
        d->bookmarksManager = new BookmarksManager(this);

        d->auxTranslatorDBus = new CAuxTranslator(this);
        d->browserControllerDBus = new CBrowserController(this);
        new AuxtranslatorAdaptor(d->auxTranslatorDBus);
        new BrowsercontrollerAdaptor(d->browserControllerDBus);
        QDBusConnection dbus = QDBusConnection::sessionBus();
        if (!dbus.registerObject(QSL("/auxTranslator"),d->auxTranslatorDBus))
            qCritical() << dbus.lastError().name() << dbus.lastError().message();
        if (!dbus.registerObject(QSL("/browserController"),d->browserControllerDBus))
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

        d->webProfile = new QWebEngineProfile(QSL("jpreader"),this);

        QString fs = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

        if (fs.isEmpty()) fs = QDir::homePath() + QDir::separator() + QSL(".cache")
                               + QDir::separator() + QSL("jpreader");
        if (!fs.endsWith(QDir::separator())) fs += QDir::separator();

        QString fcache = fs + QSL("cache") + QDir::separator();
        d->webProfile->setCachePath(fcache);
        QString fdata = fs + QSL("local_storage") + QDir::separator();
        d->webProfile->setPersistentStoragePath(fdata);

        d->webProfile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        d->webProfile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

        d->webProfile->setSpellCheckEnabled(false);

        d->webProfile->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled,true);
        d->webProfile->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,true);

        connect(d->webProfile, &QWebEngineProfile::downloadRequested,
                d->downloadManager.data(), &CDownloadManager::handleDownload);
        d->webProfile->setUrlRequestInterceptor(new CSpecUrlInterceptor(this));
        connect(d->webProfile->cookieStore(), &QWebEngineCookieStore::cookieAdded,
                this, &CGlobalControl::cookieAdded);
        connect(d->webProfile->cookieStore(), &QWebEngineCookieStore::cookieRemoved,
                this, &CGlobalControl::cookieRemoved);
        d->webProfile->cookieStore()->loadAllCookies();

        d->webProfile->installUrlSchemeHandler(CMagicFileSchemeHandler::getScheme().toUtf8(),new CMagicFileSchemeHandler(this));

        d->translatorCache = new CTranslatorCache(this);
        QString tcache = fs + QSL("translator_cache") + QDir::separator();
        d->translatorCache->setCachePath(tcache);
    }

    m_settings->readSettings(this);
    if (gSet->settings()->createCoredumps) {
        // create core dumps on segfaults
        rlimit rlp{};
        getrlimit(RLIMIT_CORE, &rlp);
        rlp.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlp);
    }

    if (!cliMode) {
        d->dictManager = new ZDict::ZDictController(this);

        d->auxNetManager = new QNetworkAccessManager(this);
        d->auxNetManager->setCookieJar(new CNetworkCookieJar(d->auxNetManager));

        d->tabsListTimer.setInterval(CDefaults::tabListSavePeriod);
        connect(&(d->tabsListTimer), &QTimer::timeout, settings(), &CSettings::writeTabsList);
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
            d->dictManager->loadDictionaries(settings()->dictPaths);
        });

        QApplication::setStyle(new CSpecMenuStyle);
        QApplication::setQuitOnLastWindowClosed(false);

        QVector<QUrl> openUrls;
        for (int i=1;i<QCoreApplication::arguments().count();i++) {
            QUrl u(QCoreApplication::arguments().at(i));
            if (!u.isEmpty() && u.isValid())
                openUrls << u;
        }

        addMainWindowEx(false,true,openUrls);

        qInfo() << "Initialization time, ms: " << initTime.elapsed();

    } else {
        if (!d->cliWorker->parseArguments())
            ::exit(1);

        QMetaObject::invokeMethod(d->cliWorker.data(),&CCLIWorker::start,Qt::QueuedConnection);
    }
}

void CGlobalControl::preinit(int &argc, char *argv[], bool *cliMode)
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

bool CGlobalControl::setupIPC()
{
    Q_D(CGlobalControl);
    if (!d->ipcServer.isNull()) return false;

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
            d->ipcServer.reset(new QLocalServer());
            d->ipcServer->listen(serverName);
            connect(d->ipcServer.data(), &QLocalServer::newConnection, this, &CGlobalControl::ipcMessageReceived);
        } else {
            sendIPCMessage(socket.data(),QSL("newWindow"));
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

    QString s = QSL("%1%2").arg(msg,QString::fromLatin1(CDefaults::ipcEOF));
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
        imsg+=QSL("%1. %2\n").arg(i+1).arg(errors.at(i));
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
    std::for_each(std::execution::par,d->createdFiles.constBegin(),d->createdFiles.constEnd(),
                  [](const QString& fname){
        QFile f(fname);
        f.remove();
    });
    d->createdFiles.clear();
}

QString CGlobalControl::makeTmpFileName(const QString& suffix, bool withDir)
{
    QString res = QUuid::createUuid().toString().remove(QRegularExpression(QSL("[^a-z,A-Z,0,1-9,-]")));
    if (!suffix.isEmpty())
        res.append(QSL(".%1").arg(suffix));
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
    m_settings->setTranslationEngine(engine);
}

void CGlobalControl::addTranslatorStatistics(CStructures::TranslationEngine engine, int textLength)
{
    if (textLength < 0) {
        Q_EMIT translationStatisticsChanged();
    } else {
        m_settings->translatorStatistics[engine][QDate::currentDate()] += static_cast<quint64>(textLength);
    }
}

QNetworkReply *CGlobalControl::auxNetworkAccessManagerGet(const QNetworkRequest &request)
{
    Q_D(const CGlobalControl);
    QNetworkRequest req = request;
    if (!m_settings->userAgent.isEmpty())
        req.setRawHeader("User-Agent", m_settings->userAgent.toLatin1());
    return d->auxNetManager->get(req);
}

QNetworkReply *CGlobalControl::auxNetworkAccessManagerPost(const QNetworkRequest &request, const QByteArray &data)
{
    Q_D(const CGlobalControl);
    QNetworkRequest req = request;
    if (!m_settings->userAgent.isEmpty())
        req.setRawHeader("User-Agent", m_settings->userAgent.toLatin1());
    return d->auxNetManager->post(req,data);
}

QWebEngineProfile *CGlobalControl::webProfile() const
{
    Q_D(const CGlobalControl);
    return d->webProfile;
}

bool CGlobalControl::exportCookies(const QString &filename, const QUrl &baseUrl, const QList<int>& cookieIndexes)
{
    Q_D(const CGlobalControl);

    if (filename.isEmpty() ||
            (baseUrl.isEmpty() && cookieIndexes.isEmpty()) ||
            (!baseUrl.isEmpty() && !cookieIndexes.isEmpty())) return false;

    auto *cj = qobject_cast<CNetworkCookieJar *>(d->auxNetManager->cookieJar());
    if (cj==nullptr) return false;
    QList<QNetworkCookie> cookiesList;
    if (baseUrl.isEmpty()) {
        const auto all = cj->getAllCookies();
        cookiesList.reserve(cookieIndexes.count());
        for (const auto idx : cookieIndexes)
            cookiesList.append(all.at(idx));
    } else {
        cookiesList = cj->cookiesForUrl(baseUrl);
    }

    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QTextStream fs(&f);

    for (const auto &cookie : qAsConst(cookiesList)) {
        fs << cookie.domain()
           << '\t'
           << CGenericFuncs::bool2str2(cookie.domain().startsWith('.'))
           << '\t'
           << cookie.path()
           << '\t'
           << CGenericFuncs::bool2str2(cookie.isSecure())
           << '\t'
           << cookie.expirationDate().toSecsSinceEpoch()
           << '\t'
           << cookie.name()
           << '\t'
           << cookie.value()
           << Qt::endl;
    }
    fs.flush();
    f.close();

    return true;
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

    if (gSet==nullptr || m_ui.isNull()) return nullptr;

    auto *mainWindow = new CMainWindow(withSearch,withViewer,viewerUrls);
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
    mainWindow->menuSettings->addAction(m_ui->actionOverrideTransFont);
    mainWindow->menuSettings->addAction(m_ui->actionOverrideTransFontColor);
    mainWindow->menuSettings->addSeparator();
    mainWindow->menuSettings->addAction(m_ui->actionUseProxy);

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

void CGlobalControl::settingsTab()
{
    m_settings->settingsTab();
}

void CGlobalControl::translationStatisticsTab()
{
    auto *dlg = CTranslatorStatisticsTab::instance();
    if (dlg!=nullptr) {
        dlg->activateWindow();
        dlg->setTabFocused();
    }
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
    QList<QAction* > res;
    if (m_ui.isNull()) return res;

    return m_ui->languageSelector->actions();
}

QList<QAction *> CGlobalControl::getSubsentencesModeActions() const
{
    QList<QAction* > res;
    if (m_ui.isNull()) return res;

    return m_ui->subsentencesMode->actions();
}

bool CGlobalControl::isBlockTabCloseActive() const
{
    Q_D(const CGlobalControl);
    return d->blockTabCloseActive;
}

void CGlobalControl::setFileDialogNewFolderName(const QString &name)
{
    Q_D(const CGlobalControl);
    d->uiTranslator->setFileDialogNewFolderName(name);
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

ZDict::ZDictController *CGlobalControl::dictionaryManager() const
{
    Q_D(const CGlobalControl);
    return d->dictManager;
}

CTranslatorCache *CGlobalControl::translatorCache() const
{
    Q_D(const CGlobalControl);
    return d->translatorCache;
}

CDownloadWriter *CGlobalControl::downloadWriter() const
{
    Q_D(const CGlobalControl);
    return d->downloadWriter;
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
    if (item.url.toString().startsWith(QSL("data:"),Qt::CaseInsensitive)) return;

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
    if (cmd.first().startsWith(QSL("newWindow"))) {
        addMainWindow();
    } else if (cmd.first().startsWith(QSL("debugRestart"))) {
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
    auto *mw = qobject_cast<CMainWindow*>(now->window());
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

void CGlobalControl::stopAndCloseTranslators()
{
    Q_D(CGlobalControl);
    QElapsedTimer tmr;

    Q_EMIT stopWorkers();

    tmr.start();
    while (!d->translatorPool.isEmpty() &&
           tmr.elapsed() < CDefaults::translatorMaxShutdownTimeMS) {
        CGenericFuncs::processedMSleep(CDefaults::translatorWaitGranularityMS);
    }
    CGenericFuncs::processedMSleep(CDefaults::translatorWaitGranularityMS);

    if (!d->translatorPool.isEmpty()) {
        Q_EMIT terminateWorkers();
        tmr.start();
        while (!d->translatorPool.isEmpty() &&
               tmr.elapsed() < CDefaults::translatorMaxShutdownTimeMS) {
            CGenericFuncs::processedMSleep(CDefaults::translatorWaitGranularityMS);
        }
        CGenericFuncs::processedMSleep(CDefaults::translatorWaitGranularityMS);
        d->translatorPool.clear(); // Just stop problematic translators, do not try to free memory correctly
    }
}

void CGlobalControl::cleanupAndExit()
{
    Q_D(CGlobalControl);
    if (d->cleaningState) return;
    d->cleaningState = true;

    if (!m_ui.isNull()) {
        m_settings->clearTabsList();
        m_settings->writeSettings();

        if (d->mainWindows.count()>0) {
            for (CMainWindow* w : qAsConst(d->mainWindows)) {
                if (w)
                    w->close();
            }
        }

        m_ui->gctxTranHotkey.unsetShortcut();
    }
    cleanTmpFiles();

    stopAndCloseTranslators();

    CPDFWorker::freePdfToText();

    if (!m_ui.isNull()) {
        // free global objects
        d->logWindow.reset(nullptr);
        d->downloadManager.reset(nullptr);
        d->auxDictionary.reset(nullptr);
        d->lightTranslator.reset(nullptr);
        d->ipcServer->close();
        d->ipcServer.reset(nullptr);
    }

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
    if (!url.scheme().startsWith(QSL("http"),Qt::CaseInsensitive)) return false;

    filter.clear();
    const QString u = url.toString(CSettings::adblockUrlFmt);

    if (d->adblockWhiteList.contains(u)) return false;

    const auto *it = std::find_if(std::execution::par,d->adblock.constBegin(),d->adblock.constEnd(),
                                  [u](const CAdBlockRule& rule){
        return rule.networkMatch(u);
    });
    if (it != d->adblock.constEnd()) {
        filter = it->filter();
        return true;
    }

    Q_EMIT addAdBlockWhiteListUrl(u);

    return false;
}

bool CGlobalControl::isScriptBlocked(const QUrl &url, const QUrl& origin)
{
    static const QStringList allowedScheme({ QSL("chrome"), QSL("about"),
                                             QSL("chrome-extension"),
                                             CMagicFileSchemeHandler::getScheme() });

    Q_D(const CGlobalControl);

    if (!settings()->useNoScript) return false;
    if (url.isRelative() || url.isLocalFile()
            || (url.scheme().toLower() == CMagicFileSchemeHandler::getScheme().toLower())) return false;
    if (allowedScheme.contains(url.scheme(),Qt::CaseInsensitive)) return false;

    const QString host = url.host();
    const QString key = origin.toString(CSettings::adblockUrlFmt);

    Q_EMIT addNoScriptPageHost(key, host);

    return !(d->noScriptWhiteList.contains(host));
}

void CGlobalControl::adblockAppend(const QString& url)
{
    adblockAppend(CAdBlockRule(url,QString()));
}

void CGlobalControl::adblockAppend(const CAdBlockRule& url, bool fast)
{
    Q_D(CGlobalControl);

    d->adblockModifyMutex.lock();
    d->adblock.append(url);
    d->adblockModifyMutex.unlock();

    if (!fast) {
        d->clearAdblockWhiteList();
        Q_EMIT adblockRulesUpdated();
    }
}

void CGlobalControl::adblockAppend(const QVector<CAdBlockRule> &urls)
{
    Q_D(CGlobalControl);

    d->adblockModifyMutex.lock();
    d->adblock.append(urls);
    d->adblockModifyMutex.unlock();

    d->clearAdblockWhiteList();

    Q_EMIT adblockRulesUpdated();
}

void CGlobalControl::adblockDelete(const QVector<CAdBlockRule> &rules)
{
    Q_D(CGlobalControl);

    d->adblockModifyMutex.lock();
    d->adblock.erase(std::remove_if(std::execution::par,d->adblock.begin(),d->adblock.end(),
                                    [rules](const CAdBlockRule& rule){
        return rules.contains(rule);
    }),d->adblock.end());
    d->adblockModifyMutex.unlock();

    d->clearAdblockWhiteList();

    Q_EMIT adblockRulesUpdated();
}

void CGlobalControl::adblockClear()
{
    Q_D(CGlobalControl);

    d->adblockModifyMutex.lock();
    d->adblock.clear();
    d->adblockModifyMutex.unlock();

    d->clearAdblockWhiteList();

    Q_EMIT adblockRulesUpdated();
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

bool CGlobalControl::haveSavedPassword(const QUrl &origin) const
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

void CGlobalControl::readPassword(const QUrl &origin, QString &user, QString &password) const
{
    user = QString();
    password = QString();

    QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params(QSL("kernel1024"), QSL("jpreader"));
    params.beginGroup(QSL("passwords"));
    QString key = QString::fromLatin1(url.toEncoded().toBase64());

    QString u = params.value(QSL("%1-user").arg(key),QString()).toString();
    QByteArray ba = params.value(QSL("%1-pass").arg(key),QByteArray()).toByteArray();
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

void CGlobalControl::savePassword(const QUrl &origin, const QString &user, const QString &password) const
{
    QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params(QSL("kernel1024"), QSL("jpreader"));
    params.beginGroup(QSL("passwords"));
    QString key = QString::fromLatin1(url.toEncoded().toBase64());
    params.setValue(QSL("%1-user").arg(key),user);
    params.setValue(QSL("%1-pass").arg(key),password.toUtf8().toBase64());
    params.endGroup();
}

void CGlobalControl::removePassword(const QUrl &origin) const
{
    QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params(QSL("kernel1024"), QSL("jpreader"));
    params.beginGroup(QSL("passwords"));
    QString key = QString::fromLatin1(url.toEncoded().toBase64());
    params.remove(QSL("%1-user").arg(key));
    params.remove(QSL("%1-pass").arg(key));
    params.endGroup();
}

QVector<CUserScript> CGlobalControl::getUserScriptsForUrl(const QUrl &url, bool isMainFrame, bool isContextMenu,
                                                          bool isTranslator)
{
    Q_D(CGlobalControl);

    d->userScriptsMutex.lock();

    QVector<CUserScript> scripts;

    for (auto it = d->userScripts.constBegin(), end = d->userScripts.constEnd(); it != end; ++it) {
        if (it.value().isEnabledForUrl(url) &&
                (isMainFrame || it.value().shouldRunOnSubFrames()) &&
                ((isContextMenu && it.value().shouldRunFromContextMenu()) ||
                 (!isContextMenu && !it.value().shouldRunFromContextMenu())) &&
                ((isTranslator && it.value().shouldRunByTranslator()) ||
                 (!isTranslator && !it.value().shouldRunByTranslator()))) {

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
        QString name = QSL("%1 (%2)")
                       .arg(QLocale::languageToString(locale.language()),bcp);

        // filter out unsupported codes for dialects
        if (bcp.contains('-') && !bcp.startsWith(QSL("zh"))) continue;

        // replace C locale with English
        if (bcp == QSL("en"))
            name = QSL("English (en)");

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

    if (forceMenuUpdate && !m_ui.isNull())
        m_ui->actionUseProxy->setChecked(settings()->proxyUse);
}

void CGlobalControl::clearCaches()
{
    Q_D(CGlobalControl);
    d->webProfile->clearHttpCache();
    d->translatorCache->clearCache();
}

void CGlobalControl::forceCharset()
{
    const int maxCharsetHistory = 10;

    auto *act = qobject_cast<QAction*>(sender());
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

void CGlobalControl::addWorkerToPool(CTranslator *tran)
{
    Q_D(CGlobalControl);
    QPointer<CTranslator> ptr(tran);
    d->translatorPool.append(ptr);
}

void CGlobalControl::addWorkerToPool(CDownloadWriter *writer)
{
    Q_D(CGlobalControl);
    QPointer<CDownloadWriter> ptr(writer);
    d->translatorPool.append(ptr);
}

void CGlobalControl::cleanupWorker()
{
    Q_D(CGlobalControl);

    auto *translator = qobject_cast<CTranslator *>(sender());
    if (translator)
        d->translatorPool.removeAll(translator);

    auto *downloadWriter = qobject_cast<CDownloadWriter *>(sender());
    if (downloadWriter)
        d->translatorPool.removeAll(downloadWriter);
}

QUrl CGlobalControl::createSearchUrl(const QString& text, const QString& engine) const
{
    Q_D(const CGlobalControl);
    if (d->ctxSearchEngines.isEmpty())
        return QUrl(QSL("about://blank"));

    auto it = d->ctxSearchEngines.constKeyValueBegin();
    QString url = (*it).second;
    if (engine.isEmpty() && !settings()->defaultSearchEngine.isEmpty())
        url = d->ctxSearchEngines.value(settings()->defaultSearchEngine);
    if (!engine.isEmpty() && d->ctxSearchEngines.contains(engine))
        url = d->ctxSearchEngines.value(engine);

    url.replace(QSL("%s"),text);
    url.replace(QSL("%ps"),QString::fromUtf8(QUrl::toPercentEncoding(text)));

    return QUrl::fromUserInput(url);
}

QStringList CGlobalControl::getSearchEngines() const
{
    Q_D(const CGlobalControl);
    return d->ctxSearchEngines.keys();
}
