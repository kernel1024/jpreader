#include "history.h"
#include "control.h"
#include "control_p.h"

CGlobalHistory::CGlobalHistory(QObject *parent)
    : QObject(parent)
{
}

const CUrlHolderVector &CGlobalHistory::recycleBin() const
{
    return gSet->d_func()->recycleBin;
}

const CUrlHolderVector &CGlobalHistory::mainHistory() const
{
    return gSet->d_func()->mainHistory;
}

const QStringList &CGlobalHistory::recentFiles() const
{
    return gSet->d_func()->recentFiles;
}

const QStringList &CGlobalHistory::searchHistory() const
{
    return gSet->d_func()->searchHistory;
}

void CGlobalHistory::appendSearchHistory(const QStringList& req)
{
    for(int i=0;i<req.count();i++) {
        int idx = gSet->d_func()->searchHistory.indexOf(req.at(i));
        if (idx>=0)
            gSet->d_func()->searchHistory.removeAt(idx);
        gSet->d_func()->searchHistory.insert(0,req.at(i));
    }
}

void CGlobalHistory::appendRecycled(const QString& title, const QUrl& url)
{
    int idx = gSet->d_func()->recycleBin.indexOf(CUrlHolder(title,url));
    if (idx>=0) {
        gSet->d_func()->recycleBin.move(idx,0);
    } else {
        gSet->d_func()->recycleBin.prepend(CUrlHolder(title,url));
    }

    if (gSet->d_func()->recycleBin.count() > gSet->m_settings->maxRecycled)
        gSet->d_func()->recycleBin.removeLast();

    Q_EMIT updateAllRecycleBins();
}

void CGlobalHistory::removeRecycledItem(int idx)
{
    gSet->d_func()->recycleBin.removeAt(idx);
    Q_EMIT updateAllRecycleBins();
}

void CGlobalHistory::appendMainHistory(const CUrlHolder &item)
{
    if (item.url.toString().startsWith(QSL("data:"),Qt::CaseInsensitive)) return;

    if (gSet->d_func()->mainHistory.contains(item))
        gSet->d_func()->mainHistory.removeOne(item);
    gSet->d_func()->mainHistory.prepend(item);

    while (gSet->d_func()->mainHistory.count() > gSet->m_settings->maxHistory)
        gSet->d_func()->mainHistory.removeLast();

    Q_EMIT updateAllHistoryLists();
}

bool CGlobalHistory::updateMainHistoryTitle(const CUrlHolder &item, const QString& newTitle)
{
    if (gSet->d_func()->mainHistory.contains(item)) {
        int idx = gSet->d_func()->mainHistory.indexOf(item);
        if (idx>=0) {
            gSet->d_func()->mainHistory[idx].title = newTitle;
            Q_EMIT updateAllHistoryLists();
            return true;
        }
    }
    return false;
}

void CGlobalHistory::appendRecent(const QString& filename)
{
    QFileInfo fi(filename);
    if (!fi.exists()) return;

    if (gSet->d_func()->recentFiles.contains(filename))
        gSet->d_func()->recentFiles.removeAll(filename);
    gSet->d_func()->recentFiles.prepend(filename);

    while (gSet->d_func()->recentFiles.count() > gSet->m_settings->maxRecent)
        gSet->d_func()->recentFiles.removeLast();

    Q_EMIT updateAllRecentLists();
}
