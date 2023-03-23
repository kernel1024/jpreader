#ifndef GLOBALCONTROL_H
#define GLOBALCONTROL_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include "settings.h"
#include "actions.h"
#include "zdict/zdictcontroller.h"

class CLogDisplay;
class CDownloadManager;
class CDownloadWriter;
class BookmarksManager;
class CTranslatorCache;
class CTranslator;
class CWorkerMonitor;
class CZipWriter;
class CAutofillAssistant;

class CGlobalControlPrivate;
class CGlobalContentFiltering;
class CGlobalStartup;
class CGlobalNetwork;
class CGlobalHistory;
class CGlobalBrowserFuncs;
class CGlobalUI;
class CGlobalPython;

#define gSet (CGlobalControl::instance())

class CGlobalControl : public QObject
{
    friend class CSettings;
    friend class CGlobalActions;
    friend class CGlobalContentFiltering;
    friend class CGlobalStartup;
    friend class CGlobalNetwork;
    friend class CGlobalHistory;
    friend class CGlobalBrowserFuncs;
    friend class CGlobalUI;

    friend class CSettingsTab;
    friend class CGenericFuncs;
    friend class CLogDisplay;
    friend class CLangPairModel;

    Q_OBJECT
public:
    explicit CGlobalControl(QCoreApplication *parent);
    ~CGlobalControl() override;
    static CGlobalControl* instance();
    QApplication* app(QObject *parentApp = nullptr);

    QString makeTmpFile(const QString &suffix, const QString &content);

    // Owned widgets
    CMainWindow *activeWindow() const;
    BookmarksManager *bookmarksManager() const;
    CDownloadManager* downloadManager() const;
    CWorkerMonitor* workerMonitor() const;
    CLogDisplay* logWindow() const;
    QNetworkAccessManager* auxNetworkAccessManager() const;
    const QHash<QString,QIcon> &favicons() const;
    ZDict::ZDictController* dictionaryManager() const;
    CTranslatorCache* translatorCache() const;
    CZipWriter* zipWriter() const;
    CAutofillAssistant *autofillAssistant() const;

    // Global subsystems
    const CSettings *settings() const;
    const CGlobalActions *actions() const;
    CGlobalContentFiltering *contentFilter() const;
    CGlobalStartup *startup() const;
    CGlobalNetwork *net() const;
    CGlobalHistory *history() const;
    CGlobalUI *ui() const;
    CGlobalBrowserFuncs *browser() const;
    CGlobalPython *python() const;

private:
    Q_DISABLE_COPY(CGlobalControl)
    Q_DECLARE_PRIVATE_D(dptr,CGlobalControl)

    QScopedPointer<CGlobalControlPrivate> dptr;
    QScopedPointer<CSettings,QScopedPointerDeleteLater> m_settings;
    QScopedPointer<CGlobalActions,QScopedPointerDeleteLater> m_actions;
    QScopedPointer<CGlobalContentFiltering,QScopedPointerDeleteLater> m_contentFilter;
    QScopedPointer<CGlobalStartup,QScopedPointerDeleteLater> m_startup;
    QScopedPointer<CGlobalNetwork,QScopedPointerDeleteLater> m_net;
    QScopedPointer<CGlobalHistory,QScopedPointerDeleteLater> m_history;
    QScopedPointer<CGlobalUI,QScopedPointerDeleteLater> m_ui;
    QScopedPointer<CGlobalBrowserFuncs,QScopedPointerDeleteLater> m_browser;
    QScopedPointer<CGlobalPython,QScopedPointerDeleteLater> m_python;

public Q_SLOTS:
    void writeSettings();

};

#endif // GLOBALCONTROL_H
