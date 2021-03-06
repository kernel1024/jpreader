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
    enum IndexMode { imWorkIndex, imBookmarksIndex, imTagSearchIndex };
    Q_ENUM(IndexMode)
    enum TagSearchMode { tsmTagOnly = 0, tsmTagFull = 1, tsmText = 2, tsmTagAll = 3 };
    Q_ENUM(TagSearchMode)
    enum NovelSearchLength { nslDefault = 0, nslFlash = 1, nslShort = 2, nslMedium = 3, nslLong = 4 };
    Q_ENUM(NovelSearchLength)
    enum NovelSearchRating { nsrAll = 0, nsrSafe = 1, nsrR18 = 2 };
    Q_ENUM(NovelSearchRating)

    CPixivIndexExtractor(QObject *parent, QWidget *parentWidget);
    QString workerDescription() const override;

    void setParams(const QString& pixivId, CPixivIndexExtractor::IndexMode indexMode, int maxCount,
                   const QDate& dateFrom, const QDate& dateTo,
                   CPixivIndexExtractor::TagSearchMode tagMode, bool originalOnly,
                   const QString &languageCode, CPixivIndexExtractor::NovelSearchLength novelLength,
                   CPixivIndexExtractor::NovelSearchRating novelRating);

    static bool extractorLimitsDialog(QWidget *parentWidget, const QString &title,
                                      const QString &groupTitle, bool isTagSearch,
                                      int &maxCount, QDate &dateFrom, QDate &dateTo, QString &keywords,
                                      CPixivIndexExtractor::TagSearchMode &tagMode, bool &originalOnly,
                                      QString &languageCode, CPixivIndexExtractor::NovelSearchLength &novelLength,
                                      CPixivIndexExtractor::NovelSearchRating &novelRating);

private:
    int m_maxCount { -1 };
    QString m_indexId;
    QUrlQuery m_sourceQuery;
    QDate m_dateFrom;
    QDate m_dateTo;
    QJsonArray m_list;
    QStringList m_ids;
    IndexMode m_indexMode { imWorkIndex };
    QAtomicInteger<int> m_worksImgFetch;
    QMutex m_imgMutex;

    void fetchNovelsInfo();
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
