#ifndef PIXIVINDEXEXTRACTOR_H
#define PIXIVINDEXEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QJsonObject>
#include <QMutex>
#include "abstractextractor.h"
#include "../structures.h"

class CPixivIndexExtractor : public CAbstractExtractor
{
    Q_OBJECT

public:
    enum IndexMode { WorkIndex, BookmarksIndex };
    Q_ENUM(IndexMode)

    CPixivIndexExtractor(QObject *parent = nullptr, CSnippetViewer *snv = nullptr);
    void setParams(const QString& pixivId, CPixivIndexExtractor::IndexMode mode);

private:
    QString m_authorId;
    QVector<QJsonObject> m_list;
    QStringList m_ids;
    IndexMode m_indexMode { WorkIndex };
    QAtomicInteger<int> m_worksImgFetch;
    QMutex m_imgMutex;

    static bool indexItemCompare(const QJsonObject& c1, const QJsonObject& c2);
    void fetchNovelsInfo();
    QString makeNovelInfoBlock(CStringHash* authors,
                               const QString& workId, const QString& workImgUrl,
                               const QString& title, int length,
                               const QString& author, const QString& authorId,
                               const QStringList& tags, const QString& description,
                               const QDateTime &creationDate, const QString &seriesTitle,
                               const QString &seriesId, int bookmarkCount);
    void finalizeHtml(const QUrl& origin);
    void preloadNovelCovers(const QUrl& origin);

protected:
    void startMain() override;

private Q_SLOTS:
    void profileAjax();
    void bookmarksAjax();
    void subImageFinished();
};

#endif // PIXIVINDEXEXTRACTOR_H
