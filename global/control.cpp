#include <QRegularExpression>

extern "C" {
#include <unicode/uclean.h>
}

#include "control.h"
#include "control_p.h"
#include "structures.h"
#include "contentfiltering.h"
#include "startup.h"
#include "network.h"
#include "history.h"
#include "ui.h"
#include "browserfuncs.h"
#include "mainwindow.h"

#include "utils/genericfuncs.h"
#include "translator/lighttranslator.h"
#include "translator/translatorcache.h"
#include "translator/translatorstatisticstab.h"

CGlobalControl::CGlobalControl(QCoreApplication *parent) :
    QObject(parent),
    dptr(new CGlobalControlPrivate(this)),
    m_settings(new CSettings(this)),
    m_startup(new CGlobalStartup(this)),
    m_net(new CGlobalNetwork(this))
{
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        m_actions.reset(new CGlobalActions(this));
        m_ui.reset(new CGlobalUI(this));
        m_browser.reset(new CGlobalBrowserFuncs(this));
        m_history.reset(new CGlobalHistory(this));
        m_contentFilter.reset(new CGlobalContentFiltering(this));
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

QString CGlobalControl::makeTmpFile(const QString& suffix, const QString& content)
{
    Q_D(CGlobalControl);

    QString res = QUuid::createUuid().toString().remove(QRegularExpression(QSL("[^a-z,A-Z,0,1-9,-]")));
    if (!suffix.isEmpty())
        res.append(QSL(".%1").arg(suffix));
    res = QDir::temp().absoluteFilePath(res);

    QFile f(res);
    if (!f.open(QIODevice::WriteOnly)) return QString();
    f.write(content.toUtf8());
    f.close();

    d->createdFiles.append(res);

    return res;
}

void CGlobalControl::writeSettings()
{
    m_settings->writeSettings();
}

const CSettings* CGlobalControl::settings() const
{
    return m_settings.data();
}

const CGlobalActions *CGlobalControl::actions() const
{
    return m_actions.data();
}

CGlobalContentFiltering *CGlobalControl::contentFilter() const
{
    return m_contentFilter.data();
}

CGlobalStartup *CGlobalControl::startup() const
{
    return m_startup.data();
}

CGlobalNetwork *CGlobalControl::net() const
{
    return m_net.data();
}

CGlobalHistory *CGlobalControl::history() const
{
    return m_history.data();
}

CGlobalUI *CGlobalControl::ui() const
{
    return m_ui.data();
}

CGlobalBrowserFuncs *CGlobalControl::browser() const
{
    return m_browser.data();
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

CAutofillAssistant *CGlobalControl::autofillAssistant() const
{
    Q_D(const CGlobalControl);
    return d->autofillAssistant.data();
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
