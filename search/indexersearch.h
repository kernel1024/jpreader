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

#include "abstractthreadedsearch.h"
#include "global/structures.h"

namespace CDefaults {
const int maxSearchFileSize = 50*1024*1024;
}

class CIndexerSearch : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CIndexerSearch)
public:
    explicit CIndexerSearch(QObject *parent = nullptr);
    ~CIndexerSearch() override = default;
    bool isValidConfig();
    bool isWorking() const;
    CStructures::SearchEngine getCurrentIndexerService();
    
private:
    QScopedPointer<CAbstractThreadedSearch,QScopedPointerDeleteLater> m_engine;
    QString m_query;
    QElapsedTimer m_searchTimer;

    CStructures::SearchEngine m_engineMode { CStructures::seNone };
    int m_resultCount { 0 };
    bool m_working { false };

    void processFile(const QString &filename, int &hitRate, QString &title);
    int calculateHitRate(const QString &fc);
    void setupEngine();

Q_SIGNALS:
    void searchFinished(const CStringHash &stats, const QString &query);
    void gotResult(const CStringHash& result);
    void gotError(const QString& message);
    void startThreadedSearch(const QString &qr, int maxLimit);

public Q_SLOTS:
    void engineFinished();
    void doSearch(const QString &searchTerm, const QDir &searchDir);
    void addHit(const CStringHash &meta);
    void engineError(const QString& message);
};

#endif // INDEXERSEARCH_H
