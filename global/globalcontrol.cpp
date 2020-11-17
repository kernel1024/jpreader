#include <QNetworkProxy>
#include <QAuthenticator>
#include <QMessageBox>
#include <QNetworkCookie>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QRegularExpression>
#include <QSettings>
#include <QTextCodec>

extern "C" {
#include <unicode/uclean.h>
}

#include "globalcontrol.h"
#include "globalprivate.h"
#include "structures.h"
#include "contentfiltering.h"
#include "startup.h"
#include "mainwindow.h"

#include "utils/genericfuncs.h"
#include "translator/lighttranslator.h"
#include "translator/translatorcache.h"
#include "translator/translatorstatisticstab.h"

namespace CDefaults{
const int tabCloseInterlockDelay = 500;
}
// TODO: some refactoring needed, separate network code, window management, browser logic facility

CGlobalControl::CGlobalControl(QCoreApplication *parent) :
    QObject(parent),
    dptr(new CGlobalControlPrivate(this)),
    m_settings(new CSettings(this)),
    m_contentFilter(new CGlobalContentFiltering(this)),
    m_startup(new CGlobalStartup(this))
{
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        m_ui.reset(new CGlobalUI(this));
    }
}

CGlobalControl::~CGlobalControl()
{
    qInstallMessageHandler(nullptr);
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

        qFatal("Accessing to gSet after destruction!!!");
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

bool CGlobalControl::sslCertErrors(const QSslCertificate &cert, const QStringList &errors,
                                   const CIntList &errCodes)
{
    Q_D(CGlobalControl);
    if (d->sslCertErrorInteractive) return false;

    QMessageBox mbox;
    mbox.setWindowTitle(QGuiApplication::applicationDisplayName());
    mbox.setText(tr("SSL error(s) occured while connecting to service.\n"
                    "Add this certificate to trusted list?"));
    QString imsg;
    for(int i=0;i<errors.count();i++)
        imsg+=QSL("%1. %2\n").arg(i+1).arg(errors.at(i));
    mbox.setInformativeText(imsg);

    mbox.setDetailedText(cert.toText());

    mbox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    mbox.setDefaultButton(QMessageBox::No);

    d->sslCertErrorInteractive = true;
    auto res = mbox.exec();
    if (res == QMessageBox::Yes) {
        for (const int errCode : qAsConst(errCodes)) {
            if (!m_settings->sslTrustedInvalidCerts[cert].contains(errCode))
                m_settings->sslTrustedInvalidCerts[cert].append(errCode);
        }
    }
    d->sslCertErrorInteractive = false;
    return (res == QMessageBox::Yes);
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

QList<QSslError> CGlobalControl::ignoredSslErrorsList() const
{
    QList<QSslError> expectedErrors;
    for (auto it = m_settings->sslTrustedInvalidCerts.constBegin(),
         end = m_settings->sslTrustedInvalidCerts.constEnd(); it != end; ++it) {
        for (auto iit = it.value().constBegin(), iend = it.value().constEnd(); iit != iend; ++iit) {
            expectedErrors << QSslError(static_cast<QSslError::SslError>(*iit),it.key());
        }
    }

    return expectedErrors;
}

QNetworkReply *CGlobalControl::auxNetworkAccessManagerHead(const QNetworkRequest &request)
{
    Q_D(const CGlobalControl);
    QNetworkRequest req = request;
    if (!m_settings->userAgent.isEmpty())
        req.setRawHeader("User-Agent", m_settings->userAgent.toLatin1());
    QNetworkReply *res = d->auxNetManager->head(req);
    res->ignoreSslErrors(ignoredSslErrorsList());
    return res;
}

QNetworkReply *CGlobalControl::auxNetworkAccessManagerGet(const QNetworkRequest &request)
{
    Q_D(const CGlobalControl);
    QNetworkRequest req = request;
    if (!m_settings->userAgent.isEmpty())
        req.setRawHeader("User-Agent", m_settings->userAgent.toLatin1());
    QNetworkReply *res = d->auxNetManager->get(req);
    res->ignoreSslErrors(ignoredSslErrorsList());
    return res;
}

QNetworkReply *CGlobalControl::auxNetworkAccessManagerPost(const QNetworkRequest &request, const QByteArray &data)
{
    Q_D(const CGlobalControl);
    QNetworkRequest req = request;
    if (!m_settings->userAgent.isEmpty())
        req.setRawHeader("User-Agent", m_settings->userAgent.toLatin1());
    QNetworkReply *res = d->auxNetManager->post(req,data);
    res->ignoreSslErrors(ignoredSslErrorsList());
    return res;
}

void CGlobalControl::authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator) const
{
    QString user;
    QString pass;
    readPassword(reply->url(),authenticator->realm(),user,pass);
    if (!user.isEmpty()) {
        authenticator->setUser(user);
        authenticator->setPassword(pass);
    } else {
        *authenticator = QAuthenticator();
    }
}

void CGlobalControl::proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator) const
{
    QString user;
    QString pass;
    readPassword(proxy.hostName(),authenticator->realm(),user,pass);
    if (!user.isEmpty()) {
        authenticator->setUser(user);
        authenticator->setPassword(pass);
    } else {
        *authenticator = QAuthenticator();
    }
}

void CGlobalControl::auxSSLCertError(QNetworkReply *reply, const QList<QSslError> &errors)
{
    QHash<QSslCertificate,QStringList> errStrHash;
    QHash<QSslCertificate,CIntList> errIntHash;

    for (const QSslError& err : qAsConst(errors)) {
        if (gSet->settings()->sslTrustedInvalidCerts.contains(err.certificate()) &&
                gSet->settings()->sslTrustedInvalidCerts.value(err.certificate()).contains(static_cast<int>(err.error()))) continue;

        qCritical() << "Aux SSL error: " << err.errorString();

        errStrHash[err.certificate()].append(err.errorString());
        errIntHash[err.certificate()].append(static_cast<int>(err.error()));
    }

    for (auto it = errStrHash.constBegin(), end = errStrHash.constEnd(); it != end; ++it) {
        if (!sslCertErrors(it.key(),it.value(),errIntHash.value(it.key())))
            return;
    }

    reply->ignoreSslErrors();
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

CGlobalContentFiltering *CGlobalControl::contentFilter() const
{
    return m_contentFilter.data();
}

CGlobalStartup *CGlobalControl::startup() const
{
    return m_startup.data();
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

CWorkerMonitor *CGlobalControl::workerMonitor() const
{
    Q_D(const CGlobalControl);
    return d->workerMonitor.data();
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

CZipWriter *CGlobalControl::zipWriter() const
{
    Q_D(const CGlobalControl);
    return d->zipWriter;
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
            m_startup->cleanupAndExit();
        }
    }
}

bool CGlobalControl::haveSavedPassword(const QUrl &origin, const QString &realm) const
{
    QString user;
    QString pass;
    readPassword(origin, realm, user, pass);
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
    res.setPath(QString());

    return res;
}

void CGlobalControl::readPassword(const QUrl &origin, const QString &realm,
                                  QString &user, QString &password) const
{
    user = QString();
    password = QString();

    const QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params(QSL("kernel1024"), QSL("jpreader"));
    params.beginGroup(QSL("passwords"));
    const QString key = QString::fromLatin1(url.toEncoded().toBase64());

    const QString cfgUser = params.value(QSL("%1-user").arg(key),QString()).toString();
    const QByteArray cfgPass64 = params.value(QSL("%1-pass").arg(key),QByteArray()).toByteArray();
    const QString cfgRealm = params.value(QSL("%1-realm").arg(key),QString()).toString();
    QString cfgPass;
    if (!cfgPass64.isEmpty()) {
        cfgPass = QString::fromUtf8(QByteArray::fromBase64(cfgPass64));
    }

    if (!cfgUser.isNull() && !cfgPass.isNull() && (cfgRealm == realm)) {
        user = cfgUser;
        password = cfgPass;
    }
    params.endGroup();
}

void CGlobalControl::savePassword(const QUrl &origin, const QString& realm,
                                  const QString &user, const QString &password) const
{
    QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params(QSL("kernel1024"), QSL("jpreader"));
    params.beginGroup(QSL("passwords"));
    QString key = QString::fromLatin1(url.toEncoded().toBase64());
    params.setValue(QSL("%1-user").arg(key),user);
    params.setValue(QSL("%1-realm").arg(key),realm);
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
    params.remove(QSL("%1-realm").arg(key));
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

void CGlobalControl::clearTranslatorCache()
{
    Q_D(CGlobalControl);
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
