#include <algorithm>
#include <execution>
#include "control.h"
#include "control_p.h"
#include "contentfiltering.h"
#include "utils/specwidgets.h"

CGlobalContentFiltering::CGlobalContentFiltering(QObject *parent) : QObject(parent)
{

}

bool CGlobalContentFiltering::isUrlBlocked(const QUrl& url)
{
    QString dummy = QString();
    return isUrlBlocked(url,dummy);
}

bool CGlobalContentFiltering::isUrlBlocked(const QUrl& url, QString &filter)
{
    if (!gSet->settings()->useAdblock) return false;
    if (!url.scheme().startsWith(QSL("http"),Qt::CaseInsensitive)) return false;

    filter.clear();
    const QString u = url.toString(CSettings::adblockUrlFmt);

    if (gSet->d_func()->adblockWhiteList.contains(u)) return false;

    auto it = std::find_if(std::execution::par,
                           gSet->d_func()->adblock.constBegin(),
                           gSet->d_func()->adblock.constEnd(),
                           [u](const CAdBlockRule& rule){
        return rule.networkMatch(u);
    });
    if (it != gSet->d_func()->adblock.constEnd()) {
        filter = it->filter();
        return true;
    }

    Q_EMIT addAdBlockWhiteListUrl(u);

    return false;
}

bool CGlobalContentFiltering::isScriptBlocked(const QUrl &url, const QUrl& origin)
{
    static const QStringList allowedScheme({ QSL("chrome"), QSL("about"),
                                             QSL("chrome-extension"),
                                             CMagicFileSchemeHandler::getScheme() });

    if (!gSet->settings()->useNoScript) return false;
    if (url.isRelative() || url.isLocalFile()
            || (url.scheme().toLower() == CMagicFileSchemeHandler::getScheme().toLower())) return false;
    if (allowedScheme.contains(url.scheme(),Qt::CaseInsensitive)) return false;

    const QString host = url.host();
    const QString key = origin.toString(CSettings::adblockUrlFmt);

    Q_EMIT addNoScriptPageHost(key, host);

    return !(gSet->d_func()->noScriptWhiteList.contains(host));
}

void CGlobalContentFiltering::adblockAppend(const QString& url)
{
    adblockAppend(CAdBlockRule(url,QString()));
}

void CGlobalContentFiltering::adblockAppend(const CAdBlockRule& url, bool fast)
{
    gSet->d_func()->adblockModifyMutex.lock();
    gSet->d_func()->adblock.append(url);
    gSet->d_func()->adblockModifyMutex.unlock();

    if (!fast) {
        gSet->d_func()->clearAdblockWhiteList();
        Q_EMIT adblockRulesUpdated();
    }
}

void CGlobalContentFiltering::adblockAppend(const QList<CAdBlockRule> &urls)
{
    gSet->d_func()->adblockModifyMutex.lock();
    gSet->d_func()->adblock.append(urls);
    gSet->d_func()->adblockModifyMutex.unlock();

    gSet->d_func()->clearAdblockWhiteList();

    Q_EMIT adblockRulesUpdated();
}

void CGlobalContentFiltering::adblockDelete(const QList<CAdBlockRule> &rules)
{
    gSet->d_func()->adblockModifyMutex.lock();
    gSet->d_func()->adblock.erase(std::remove_if(std::execution::par,
                                                 gSet->d_func()->adblock.begin(),
                                                 gSet->d_func()->adblock.end(),
                                    [rules](const CAdBlockRule& rule){
        return rules.contains(rule);
    }),gSet->d_func()->adblock.end());
    gSet->d_func()->adblockModifyMutex.unlock();

    gSet->d_func()->clearAdblockWhiteList();

    Q_EMIT adblockRulesUpdated();
}

void CGlobalContentFiltering::adblockClear()
{
    gSet->d_func()->adblockModifyMutex.lock();
    gSet->d_func()->adblock.clear();
    gSet->d_func()->adblockModifyMutex.unlock();

    gSet->d_func()->clearAdblockWhiteList();

    Q_EMIT adblockRulesUpdated();
}

void CGlobalContentFiltering::clearNoScriptPageHosts(const QString &origin)
{
    gSet->d_func()->noScriptMutex.lock();
    gSet->d_func()->pageScriptHosts[origin].clear();
    gSet->d_func()->noScriptMutex.unlock();
}

CStringSet CGlobalContentFiltering::getNoScriptPageHosts(const QString &origin)
{
    CStringSet res;
    gSet->d_func()->noScriptMutex.lock();
    res = gSet->d_func()->pageScriptHosts.value(origin);
    gSet->d_func()->noScriptMutex.unlock();
    return res;
}

void CGlobalContentFiltering::removeNoScriptWhitelistHost(const QString &host)
{
    gSet->d_func()->noScriptMutex.lock();
    gSet->d_func()->noScriptWhiteList.remove(host);
    gSet->d_func()->noScriptMutex.unlock();
}

void CGlobalContentFiltering::addNoScriptWhitelistHost(const QString &host)
{
    gSet->d_func()->noScriptMutex.lock();
    gSet->d_func()->noScriptWhiteList.insert(host);
    gSet->d_func()->noScriptMutex.unlock();
}

bool CGlobalContentFiltering::containsNoScriptWhitelist(const QString &host) const
{
    return gSet->d_func()->noScriptWhiteList.contains(host);
}
