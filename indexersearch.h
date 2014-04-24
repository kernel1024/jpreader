#ifndef INDEXERSEARCH_H
#define INDEXERSEARCH_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QString>
#include <QTime>
#include <QDir>
#include <QFileInfo>
#ifdef WITH_NEPOMUK
#include <kdirmodel.h>
#include <kdirlister.h>
#include <filequery.h>
#include <literalterm.h>
#endif
#include "recollsearch.h"

typedef QHash<QString, QString> QStrHash;

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
#ifdef WITH_NEPOMUK
    KDirModel engine;
    void addHit(const KFileItem &hit);
#endif
#ifdef WITH_RECOLL
    CRecollSearch *recollEngine;
#endif
    bool working;
    QBResult result;
    QString query;
    QTime searchTimer;
    int indexerSerivce;
    void addHitFS(const QFileInfo &hit);
    void processFile(const QString &filename, double &hitRate, QString &title);
    QString extractFileTitle(const QString& fc);
    double calculateHitRate(const QString &fc);
    void searchInDir(const QDir &dir, const QString &qr);

signals:
    void searchFinished(const QBResult &result, const QString &aQuery);
#ifdef WITH_RECOLL
    void recollStartSearch(const QString &qr, int maxLimit);
#endif

public slots:
    void engineFinished();
    void doSearch(const QString &searchTerm, const QDir &searchDir);
#ifdef WITH_NEPOMUK
    void nepomukNewItems(const KFileItemList &items);
#endif
    void auxAddHit(const QString &fileName);

};

#endif // INDEXERSEARCH_H
