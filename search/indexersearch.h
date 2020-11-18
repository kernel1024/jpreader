#ifndef INDEXERSEARCH_H
#define INDEXERSEARCH_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QString>
#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <QScopedPointer>

#include "global/control.h"

#ifdef WITH_THREADED_SEARCH
#include "abstractthreadedsearch.h"
#include "baloo5search.h"
#include "recollsearch.h"
#endif

namespace CDefaults {
const int maxSearchFileSize = 50*1024*1024;
}

class CIndexerSearch : public QObject
{
    Q_OBJECT
public:
    explicit CIndexerSearch(QObject *parent = nullptr);
    bool isValidConfig();
    bool isWorking() const;
    CStructures::SearchEngine getCurrentIndexerService();
    
private:
#ifdef WITH_THREADED_SEARCH
    QScopedPointer<CAbstractThreadedSearch,QScopedPointerDeleteLater> engine;
#endif
    QString m_query;
    QElapsedTimer searchTimer;
    CStructures::SearchEngine indexerSerivce;
    bool working { false };
    int resultCount { 0 };
    void processFile(const QString &filename, int &hitRate, QString &title);
    int calculateHitRate(const QString &fc);
    void searchInDir(const QDir &dir, const QString &qr);

Q_SIGNALS:
    void searchFinished(const CStringHash &stats, const QString &query);
    void gotResult(const CStringHash& result);
#ifdef WITH_THREADED_SEARCH
    void startThreadedSearch(const QString &qr, int maxLimit);
#endif

public Q_SLOTS:
    void engineFinished();
    void doSearch(const QString &searchTerm, const QDir &searchDir);
    void addHit(const CStringHash &meta);

};

#endif // INDEXERSEARCH_H
