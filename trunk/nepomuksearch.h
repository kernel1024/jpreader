#ifndef NEPOMUKSEARCH_H
#define NEPOMUKSEARCH_H

#include <QtCore>
#include <nepomuk/filequery.h>
#include <nepomuk/literalterm.h>
#include <kdirmodel.h>
#include <kdirlister.h>

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

class CNepomukSearch : public QObject
{
    Q_OBJECT
public:
    QBResult result;
    QString query;
    bool working;
    explicit CNepomukSearch(QObject *parent = 0);
    void doSearch(const QString &searchTerm, const QDir &searchDir = QDir("/"));
    
private:
    KDirModel engine;
    QTime searchTimer;
    void addHit(const KFileItem &hit);
    void addHitFS(const QFileInfo &hit);
    double calculateHitRate(const QString &filename);
    void searchInDir(const QDir &dir, const QString &qr);

signals:
    void searchFinished();
    
public slots:
    void nepomukFinished();
    void nepomukNewItems(const KFileItemList &items);

};

#endif // NEPOMUKSEARCH_H
