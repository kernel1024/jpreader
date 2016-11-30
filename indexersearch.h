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
    QStrHash stats;
    QList<QStrHash> snippets;
    bool presented;
    QBResult();
    QBResult(const QBResult& other);
    QBResult& operator=(const QBResult& other);
};

Q_DECLARE_METATYPE(QBResult)

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
    QBResult result;
    QString query;
    QTime searchTimer;
    int indexerSerivce;
    bool working;
    void addHitFS(const QFileInfo &hit, const QString &title=QString(), double rel=-1.0);
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
    void auxAddHit(const QString &fileName);
    void auxAddHitFull(const QString &fileName, const QString &title, double rel);

};

#endif // INDEXERSEARCH_H
