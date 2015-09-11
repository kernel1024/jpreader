#ifndef INDEXERSEARCH_H
#define INDEXERSEARCH_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QString>
#include <QTime>
#include <QDir>
#include <QFileInfo>

#include "globalcontrol.h"

#ifdef WITH_THREADED_SEARCH
#include "abstractthreadedsearch.h"
#include "baloo5search.h"
#include "recollsearch.h"
#endif

class QBResult
{
public:
    int sortMode;
    bool presented;
    QStrHash stats;
    QList<QStrHash> snippets;
    QBResult() { presented=false; stats.clear(); snippets.clear(); sortMode = -2; }
    QBResult(QStrHash stats, QList<QStrHash> snippets) {
        QBResult::presented = true;
        QBResult::sortMode = -2;
        QBResult::stats=stats;
        foreach (QStrHash h, snippets) {
            QBResult::snippets.append(h);
        }
    }
    inline QBResult& operator=(const QBResult& other) {
        presented = other.presented;
        stats=other.stats;
        sortMode=other.sortMode;
        snippets.clear();
        foreach (QStrHash h, other.snippets) {
            snippets.append(h);
        }
        return *this;
    }
};

class CIndexerSearch : public QObject
{
    Q_OBJECT
public:
    explicit CIndexerSearch(QObject *parent = 0);
    bool isValidConfig();
    bool isWorking();
    int getCurrentIndexerService();
    
private:
#ifdef WITH_THREADED_SEARCH
    CAbstractThreadedSearch *engine;
#endif
    bool working;
    QBResult result;
    QString query;
    QTime searchTimer;
    int indexerSerivce;
    void addHitFS(const QFileInfo &hit, const QString &title=QString(), const float rel=-1.0);
    void processFile(const QString &filename, double &hitRate, QString &title);
    QString extractFileTitle(const QString& fc);
    double calculateHitRate(const QString &fc);
    void searchInDir(const QDir &dir, const QString &qr);

signals:
    void searchFinished(const QBResult &result, const QString &aQuery);
#ifdef WITH_THREADED_SEARCH
    void startThreadedSearch(const QString &qr, int maxLimit);
#endif

public slots:
    void engineFinished();
    void doSearch(const QString &searchTerm, const QDir &searchDir);
    void auxAddHit(const QString &fileName, const QString &title=QString(), const float rel=-1.0);

};

#endif // INDEXERSEARCH_H
