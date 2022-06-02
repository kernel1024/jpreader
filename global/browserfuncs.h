#ifndef GLOBALBROWSERFUNCS_H
#define GLOBALBROWSERFUNCS_H

#include <QObject>
#include <QWebEngineProfile>
#include <QNetworkCookie>
#include <functional>

#include "structures.h"
#include "browser-utils/userscript.h"

class CGlobalBrowserFuncs : public QObject
{
    Q_OBJECT
public:
    explicit CGlobalBrowserFuncs(QObject *parent = nullptr);

    // Password management
    QUrl cleanUrlForRealm(const QUrl &origin) const;
    void readPassword(const QUrl &origin, const QString &realm, QString &user, QString &password) const;
    void savePassword(const QUrl &origin, const QString &realm, const QString &user, const QString &password) const;
    bool haveSavedPassword(const QUrl &origin, const QString &realm) const;
    void removePassword(const QUrl &origin) const;

    // Chromium
    QWebEngineProfile* webProfile() const;

    // Headless worker
    void headlessDOMWorker(const QUrl& url, const QString &javaScript,
                           const std::function<bool (const QVariant &)> &matchFunc) const;
    bool isDOMWorkerReady() const;

    // Userscripts
    QVector<CUserScript> getUserScriptsForUrl(const QUrl &url, bool isMainFrame, bool isContextMenu,
                                              bool isTranslator);
    void initUserScripts(const CStringHash& scripts);
    CStringHash getUserScripts();

    int downloadsLimit() const;

public Q_SLOTS:
    void setDownloadsLimit(int count);

};

#endif // GLOBALBROWSERFUNCS_H
