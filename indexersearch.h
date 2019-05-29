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

const int maxSearchFileSize = 50*1024*1024;

class CIndexerSearch : public QObject
{
    Q_OBJECT
public:
    explicit CIndexerSearch(QObject *parent = nullptr);
    bool isValidConfig();
    bool isWorking();
    SearchEngine getCurrentIndexerService();
    
private:
#ifdef WITH_THREADED_SEARCH
    CAbstractThreadedSearch *engine;
#endif
    QString m_query;
    QTime searchTimer;
    SearchEngine indexerSerivce;
    bool working { false };
    int resultCount { 0 };
    void processFile(const QString &filename, int &hitRate, QString &title);
    int calculateHitRate(const QString &fc);
    void searchInDir(const QDir &dir, const QString &qr);

signals:
    void searchFinished(const CStringHash &stats, const QString &query);
    void gotResult(const CStringHash& result);
#ifdef WITH_THREADED_SEARCH
    void startThreadedSearch(const QString &qr, int maxLimit);
#endif

public slots:
    void engineFinished();
    void doSearch(const QString &searchTerm, const QDir &searchDir);
    void addHit(const CStringHash &meta);

};

#endif // INDEXERSEARCH_H
