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

class CIndexerSearch : public QObject
{
    Q_OBJECT
public:
    explicit CIndexerSearch(QObject *parent = nullptr);
    bool isValidConfig();
    bool isWorking();
    int getCurrentIndexerService();
    
private:
#ifdef WITH_THREADED_SEARCH
    CAbstractThreadedSearch *engine;
#endif
    QString m_query;
    QTime searchTimer;
    int indexerSerivce;
    bool working;
    int resultCount;
    void addHitFS(const QFileInfo &hit, const QString &title=QString(),
                  double rel=-1.0, const QString& snippet = QString());
    void processFile(const QString &filename, double &hitRate, QString &title);
    double calculateHitRate(const QString &fc);
    void searchInDir(const QDir &dir, const QString &qr);

signals:
    void searchFinished(const QStrHash &stats, const QString &query);
    void gotResult(const QStrHash& result);
#ifdef WITH_THREADED_SEARCH
    void startThreadedSearch(const QString &qr, int maxLimit);
#endif

public slots:
    void engineFinished();
    void doSearch(const QString &searchTerm, const QDir &searchDir);
    void auxAddHit(const QString &fileName, const QString &title, double rel, const QString& snippet);

};

#endif // INDEXERSEARCH_H
