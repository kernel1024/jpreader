#ifndef GLOBALHISTORY_H
#define GLOBALHISTORY_H

#include <QObject>

#include "structures.h"

class CGlobalHistory : public QObject
{
    Q_OBJECT
public:
    explicit CGlobalHistory(QObject *parent = nullptr);

    const CUrlHolderVector &recycleBin() const;
    const CUrlHolderVector &mainHistory() const;
    const QStringList &recentFiles() const;
    const QStringList &searchHistory() const;
    void appendRecycled(const QString &title, const QUrl &url);
    void removeRecycledItem(int idx);
    void appendSearchHistory(const QStringList &req);
    void appendMainHistory(const CUrlHolder &item);
    bool updateMainHistoryTitle(const CUrlHolder &item, const QString &newTitle);
    void appendRecent(const QString &filename);

Q_SIGNALS:
    void updateAllBookmarks();
    void updateAllRecycleBins();
    void updateAllCharsetLists();
    void updateAllQueryLists();
    void updateAllHistoryLists();
    void updateAllRecentLists();
    void updateAllLanguagesLists();

};

#endif // GLOBALHISTORY_H
