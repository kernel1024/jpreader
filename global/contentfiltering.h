#ifndef GLOBALCONTENTFILTERING_H
#define GLOBALCONTENTFILTERING_H

#include <QObject>
#include "browser-utils/adblockrule.h"
#include "global/structures.h"

class CGlobalContentFiltering : public QObject
{
    Q_OBJECT
public:
    explicit CGlobalContentFiltering(QObject *parent = nullptr);

    // Ad-block
    bool isUrlBlocked(const QUrl &url);
    bool isUrlBlocked(const QUrl &url, QString &filter);
    void adblockAppend(const QString &url);
    void adblockAppend(const CAdBlockRule &url, bool fast = false);
    void adblockAppend(const QVector<CAdBlockRule> &urls);
    void adblockDelete(const QVector<CAdBlockRule> &rules);
    void adblockClear();

    // No-Script
    bool isScriptBlocked(const QUrl &url, const QUrl &origin);
    void clearNoScriptPageHosts(const QString& origin);
    CStringSet getNoScriptPageHosts(const QString& origin);
    void removeNoScriptWhitelistHost(const QString& host);
    void addNoScriptWhitelistHost(const QString& host);
    bool containsNoScriptWhitelist(const QString& host) const;

Q_SIGNALS:
    void addAdBlockWhiteListUrl(const QString& url);
    void addNoScriptPageHost(const QString& origin, const QString& host);
    void adblockRulesUpdated();

};

#endif // GLOBALCONTENTFILTERING_H
