#include <chrono>
#include <QSettings>

#include "browserfuncs.h"
#include "control.h"
#include "control_p.h"
#include "utils/specwidgets.h"
#include "utils/genericfuncs.h"

using namespace std::chrono_literals;

namespace CDefaults {
const auto domWorkerCheckInterval = 500ms;
const auto domWorkerReplyTimeout = 60s;
}

CGlobalBrowserFuncs::CGlobalBrowserFuncs(QObject *parent)
    : QObject(parent)
{
}

QWebEngineProfile *CGlobalBrowserFuncs::webProfile() const
{
    return gSet->d_func()->webProfile;
}

bool CGlobalBrowserFuncs::haveSavedPassword(const QUrl &origin, const QString &realm) const
{
    QString user;
    QString pass;
    readPassword(origin, realm, user, pass);
    return (!user.isEmpty() || !pass.isEmpty());
}

QUrl CGlobalBrowserFuncs::cleanUrlForRealm(const QUrl &origin) const
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

void CGlobalBrowserFuncs::readPassword(const QUrl &origin, const QString &realm,
                                  QString &user, QString &password) const
{
    user = QString();
    password = QString();

    const QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params;
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

void CGlobalBrowserFuncs::savePassword(const QUrl &origin, const QString& realm,
                                  const QString &user, const QString &password) const
{
    QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params;
    params.beginGroup(QSL("passwords"));
    QString key = QString::fromLatin1(url.toEncoded().toBase64());
    params.setValue(QSL("%1-user").arg(key),user);
    params.setValue(QSL("%1-realm").arg(key),realm);
    params.setValue(QSL("%1-pass").arg(key),password.toUtf8().toBase64());
    params.endGroup();
}

void CGlobalBrowserFuncs::removePassword(const QUrl &origin) const
{
    QUrl url = cleanUrlForRealm(origin);
    if (!url.isValid() || url.isEmpty()) return;

    QSettings params;
    params.beginGroup(QSL("passwords"));
    QString key = QString::fromLatin1(url.toEncoded().toBase64());
    params.remove(QSL("%1-user").arg(key));
    params.remove(QSL("%1-realm").arg(key));
    params.remove(QSL("%1-pass").arg(key));
    params.endGroup();
}

QVector<CUserScript> CGlobalBrowserFuncs::getUserScriptsForUrl(const QUrl &url, bool isMainFrame, bool isContextMenu,
                                                          bool isTranslator)
{
    gSet->d_func()->userScriptsMutex.lock();

    QVector<CUserScript> scripts;

    for (auto it = gSet->d_func()->userScripts.constBegin(),
         end = gSet->d_func()->userScripts.constEnd(); it != end; ++it) {
        if (it.value().isEnabledForUrl(url) &&
                (isMainFrame || it.value().shouldRunOnSubFrames()) &&
                ((isContextMenu && it.value().shouldRunFromContextMenu()) ||
                 (!isContextMenu && !it.value().shouldRunFromContextMenu())) &&
                ((isTranslator && it.value().shouldRunByTranslator()) ||
                 (!isTranslator && !it.value().shouldRunByTranslator()))) {

            scripts.append(it.value());
        }
    }

    gSet->d_func()->userScriptsMutex.unlock();

    return scripts;
}

void CGlobalBrowserFuncs::initUserScripts(const CStringHash &scripts)
{
    gSet->d_func()->userScriptsMutex.lock();

    gSet->d_func()->userScripts.clear();
    for (auto it = scripts.constBegin(), end = scripts.constEnd(); it != end; ++it)
        gSet->d_func()->userScripts[it.key()] = CUserScript(it.key(), it.value());

    gSet->d_func()->userScriptsMutex.unlock();
}

CStringHash CGlobalBrowserFuncs::getUserScripts()
{
    gSet->d_func()->userScriptsMutex.lock();

    CStringHash res;

    for (auto it = gSet->d_func()->userScripts.constBegin(),
         end = gSet->d_func()->userScripts.constEnd(); it != end; ++it)
        res[it.key()] = (*it).getSource();

    gSet->d_func()->userScriptsMutex.unlock();

    return res;
}

void CGlobalBrowserFuncs::headlessDOMWorker(const QUrl &url, const QString &javaScript,
                                            const std::function<bool (const QVariant &)> &matchFunc) const
{
    if (!isDOMWorkerReady()) return;

    QMutexLocker locker(&(gSet->d_func()->domWorkerMutex));

    QTimer retryTimer;
    QTimer failTimer;
    QPointer<QEventLoop> eventLoop(new QEventLoop());

    retryTimer.setInterval(CDefaults::domWorkerCheckInterval);
    retryTimer.setSingleShot(false);
    failTimer.setInterval(CDefaults::domWorkerReplyTimeout);
    failTimer.setSingleShot(true);
    connect(&failTimer,&QTimer::timeout,eventLoop.data(),[eventLoop,url](){
        qWarning() << QSL("Failed to load page '%1' in DOM worker, aborting.").arg(url.toString());
        if (eventLoop)
            eventLoop->quit();
    });

    QMetaObject::invokeMethod(gSet->d_func()->domWorker->page(),[url](){
        gSet->d_func()->domWorker->load(url);
    });

    connect(&retryTimer,&QTimer::timeout,gSet->d_func()->domWorker->page(),
            [javaScript,eventLoop,matchFunc](){
        if (eventLoop.isNull())
            return;

        gSet->d_func()->domWorker->page()->runJavaScript(javaScript,
                                                         [eventLoop,matchFunc](const QVariant &result) {
            if (matchFunc(result))
                eventLoop->quit();
        });
    });

    failTimer.start();
    retryTimer.start();
    eventLoop->exec();
    retryTimer.stop();
    failTimer.stop();
    eventLoop->deleteLater();
}

bool CGlobalBrowserFuncs::isDOMWorkerReady() const
{
    return !(gSet->d_func()->domWorker.isNull());
}
