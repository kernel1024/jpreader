#ifndef PIXIVINDEXEXTRACTOR_H
#define PIXIVINDEXEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include "abstractextractor.h"
#include "global/structures.h"

class CPixivIndexExtractor : public CAbstractExtractor
{
    Q_OBJECT

public:
    enum ExtractorMode { emNovels, emArtworks };
    Q_ENUM(ExtractorMode)
    enum IndexMode { imWorkIndex, imBookmarksIndex, imTagSearchIndex };
    Q_ENUM(IndexMode)
    enum TagSearchMode { tsmTagOnly = 0, tsmTagFull = 1, tsmText = 2, tsmTagAll = 3 };
    Q_ENUM(TagSearchMode)
    enum NovelSearchLength { nslDefault = 0, nslFlash = 1, nslShort = 2, nslMedium = 3, nslLong = 4 };
    Q_ENUM(NovelSearchLength)
    enum SearchRating { srAll = 0, srSafe = 1, srR18 = 2 };
    Q_ENUM(SearchRating)

    CPixivIndexExtractor(QObject *parent);
    QString workerDescription() const override;

    void setParams(const QString& pixivId, CPixivIndexExtractor::ExtractorMode extractorMode,
                   CPixivIndexExtractor::IndexMode indexMode, int maxCount,
                   const QDate& dateFrom, const QDate& dateTo,
                   CPixivIndexExtractor::TagSearchMode tagMode, bool originalOnly,
                   const QString &languageCode, CPixivIndexExtractor::NovelSearchLength novelLength,
                   CPixivIndexExtractor::SearchRating novelRating);

    static bool extractorLimitsDialog(QWidget *parentWidget, const QString &title,
                                      const QString &groupTitle, bool isTagSearch,
                                      int &maxCount, QDate &dateFrom, QDate &dateTo, QString &keywords,
                                      CPixivIndexExtractor::TagSearchMode &tagMode, bool &originalOnly,
                                      QString &languageCode, CPixivIndexExtractor::NovelSearchLength &novelLength,
                                      CPixivIndexExtractor::SearchRating &novelRating);

private:
    int m_maxCount { -1 };
    QString m_indexId;
    QUrlQuery m_sourceQuery;
    QDate m_dateFrom;
    QDate m_dateTo;
    QJsonArray m_list;
    QStringList m_ids;
    QStringList m_illustsIds;
    QStringList m_mangaIds;
    IndexMode m_indexMode { imWorkIndex };
    ExtractorMode m_extractorMode { emNovels };
    QAtomicInteger<int> m_worksImgFetch;
    QMutex m_imgMutex;

    void fetchNovelsInfo();
    void fetchArtworksInfo();
    void showIndexResult(const QUrl& origin);
    void preloadNovelCovers(const QUrl& origin);

protected:
    void startMain() override;

public Q_SLOTS:
    void subImageFinished();

Q_SIGNALS:
    void auxImgLoadFinished(const QUrl& url, const QString& data);

private Q_SLOTS:
    void profileAjax();
    void bookmarksAjax();
    void searchAjax();
};

#endif // PIXIVINDEXEXTRACTOR_H
