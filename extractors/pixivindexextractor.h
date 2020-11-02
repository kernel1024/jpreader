#ifndef PIXIVINDEXEXTRACTOR_H
#define PIXIVINDEXEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonObject>
#include <QMutex>
#include "abstractextractor.h"
#include "../structures.h"

class CPixivIndexExtractor : public CAbstractExtractor
{
    Q_OBJECT

public:
    enum IndexMode { WorkIndex, BookmarksIndex, TagSearchIndex };
    Q_ENUM(IndexMode)

    CPixivIndexExtractor(QObject *parent, QWidget *parentWidget);
    void setParams(const QString& pixivId, const QString& sourceQuery,
                   CPixivIndexExtractor::IndexMode mode);

private:
    QString m_indexId;
    QUrlQuery m_sourceQuery;
    QVector<QJsonObject> m_list;
    QStringList m_ids;
    IndexMode m_indexMode { WorkIndex };
    QAtomicInteger<int> m_worksImgFetch;
    QMutex m_imgMutex;

    void fetchNovelsInfo();
    void showIndexResult(const QUrl& origin);
    void preloadNovelCovers(const QUrl& origin);

protected:
    void startMain() override;

private Q_SLOTS:
    void profileAjax();
    void bookmarksAjax();
    void searchAjax();
    void subImageFinished();
};

#endif // PIXIVINDEXEXTRACTOR_H
