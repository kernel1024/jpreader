#include <algorithm>
#include <execution>

#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QBuffer>

#include "pixivindexextractor.h"
#include "global/control.h"
#include "global/network.h"
#include "global/ui.h"
#include "utils/genericfuncs.h"
#include "utils/pixivindextab.h"
#include "utils/pixivindexlimitsdialog.h"

namespace CDefaults {
const int pixivBookmarksFetchCount = 24;
}

CPixivIndexExtractor::CPixivIndexExtractor(QObject *parent)
    : CAbstractExtractor(parent)
{
}

void CPixivIndexExtractor::setParams(const QString &pixivId, ExtractorMode extractorMode,
                                     CPixivIndexExtractor::IndexMode indexMode, int maxCount,
                                     const QDate &dateFrom, const QDate &dateTo,
                                     CPixivIndexExtractor::TagSearchMode tagMode, bool originalOnly,
                                     const QString &languageCode, CPixivIndexExtractor::NovelSearchLength novelLength,
                                     CPixivIndexExtractor::SearchRating rating, ArtworkSearchType artworkType,
                                     ArtworkSearchSize artworkSize, ArtworkSearchRatio artworkRatio,
                                     const QString &artworkCreationTool,
                                     bool hideAIWorks,
                                     CPixivIndexExtractor::NovelSearchLengthMode novelLengthMode)
{
    static const QDate minimalDate = QDate(2000,1,1);

    m_extractorMode = extractorMode;
    m_indexMode = indexMode;
    m_indexId = pixivId;
    m_maxCount = maxCount;
    m_dateFrom = dateFrom;
    m_dateTo = dateTo;

    const bool artworksMode = (m_extractorMode == ExtractorMode::emArtworks);
    m_sourceQuery.clear();
    if (indexMode == IndexMode::imTagSearchIndex) {
        m_sourceQuery.addQueryItem(QSL("word"),m_indexId);
        m_sourceQuery.addQueryItem(QSL("order"),QSL("date_d"));
        switch (rating) {
            case SearchRating::srAll: m_sourceQuery.addQueryItem(QSL("mode"),QSL("all")); break;
            case SearchRating::srSafe: m_sourceQuery.addQueryItem(QSL("mode"),QSL("safe")); break;
            case SearchRating::srR18: m_sourceQuery.addQueryItem(QSL("mode"),QSL("r18")); break;
        }

        QDate dFrom = m_dateFrom;
        QDate dTo = m_dateTo;
        if (!dFrom.isNull() || !dTo.isNull()) {
            if (dFrom.isNull()) dFrom = minimalDate;
            if (dTo.isNull()) dTo = QDate::currentDate();
            m_sourceQuery.addQueryItem(QSL("scd"),dFrom.toString(Qt::ISODate));
            m_sourceQuery.addQueryItem(QSL("ecd"),dTo.toString(Qt::ISODate));
        }

        TagSearchMode s_mode = tagMode;
        if (artworksMode && (s_mode == TagSearchMode::tsmText))
            s_mode = TagSearchMode::tsmTagAll;
        switch (tagMode) {
            case TagSearchMode::tsmTagOnly: m_sourceQuery.addQueryItem(QSL("s_mode"),QSL("s_tag_only")); break;
            case TagSearchMode::tsmTagFull: m_sourceQuery.addQueryItem(QSL("s_mode"),QSL("s_tag_full")); break; // default
            case TagSearchMode::tsmText: m_sourceQuery.addQueryItem(QSL("s_mode"),QSL("s_tc")); break;
            case TagSearchMode::tsmTagAll: m_sourceQuery.addQueryItem(QSL("s_mode"),QSL("s_tag")); break;
        }

        if (artworksMode) {
            switch (artworkType) {
                case ArtworkSearchType::astAll: m_sourceQuery.addQueryItem(QSL("type"),QSL("all")); break;
                case ArtworkSearchType::astIllustAndUgoira:
                    m_sourceQuery.addQueryItem(QSL("type"),QSL("illust_and_ugoira")); break;
                case ArtworkSearchType::astIllust: m_sourceQuery.addQueryItem(QSL("type"),QSL("illust")); break;
                case ArtworkSearchType::astManga: m_sourceQuery.addQueryItem(QSL("type"),QSL("manga")); break;
                case ArtworkSearchType::astUgoira: m_sourceQuery.addQueryItem(QSL("type"),QSL("ugoira")); break;
            }
            switch (artworkSize) {
                case ArtworkSearchSize::ass3kPlus:
                    m_sourceQuery.addQueryItem(QSL("wlt"),QSL("3000"));
                    m_sourceQuery.addQueryItem(QSL("hlt"),QSL("3000"));
                    break;
                case ArtworkSearchSize::ass2k:
                    m_sourceQuery.addQueryItem(QSL("wlt"),QSL("1000"));
                    m_sourceQuery.addQueryItem(QSL("wgt"),QSL("2000"));
                    m_sourceQuery.addQueryItem(QSL("hlt"),QSL("1000"));
                    m_sourceQuery.addQueryItem(QSL("hgt"),QSL("2000"));
                    break;
                case ArtworkSearchSize::ass1k:
                    m_sourceQuery.addQueryItem(QSL("wgt"),QSL("999"));
                    m_sourceQuery.addQueryItem(QSL("hgt"),QSL("999"));
                    break;
                case ArtworkSearchSize::assAll: break;
            }
            switch (artworkRatio) {
                case ArtworkSearchRatio::asrHorizontal: m_sourceQuery.addQueryItem(QSL("ratio"),QSL("0.5")); break;
                case ArtworkSearchRatio::asrVertical: m_sourceQuery.addQueryItem(QSL("ratio"),QSL("-0.5")); break;
                case ArtworkSearchRatio::asrSquare: m_sourceQuery.addQueryItem(QSL("ratio"),QSL("0")); break;
                case ArtworkSearchRatio::asrAll: break;
            }
            if (!artworkCreationTool.isEmpty())
                m_sourceQuery.addQueryItem(QSL("tool"),artworkCreationTool);

        } else {
            switch (novelLength) {
                case NovelSearchLength::nslDefault:
                    break;
                case NovelSearchLength::nslFlash:
                    if (novelLengthMode == NovelSearchLengthMode::nslmWords) {
                        m_sourceQuery.addQueryItem(QSL("wlt"), QSL("0"));
                        m_sourceQuery.addQueryItem(QSL("wgt"), QSL("4999"));
                    } else if (novelLengthMode == NovelSearchLengthMode::nslmTime) {
                        m_sourceQuery.addQueryItem(QSL("rlt"), QSL("0"));
                        m_sourceQuery.addQueryItem(QSL("rgt"), QSL("9"));
                    } else {
                        m_sourceQuery.addQueryItem(QSL("tlt"), QSL("0"));
                        m_sourceQuery.addQueryItem(QSL("tgt"), QSL("4999"));
                    }
                    break;
                case NovelSearchLength::nslShort:
                    if (novelLengthMode == NovelSearchLengthMode::nslmWords) {
                        m_sourceQuery.addQueryItem(QSL("wlt"), QSL("5000"));
                        m_sourceQuery.addQueryItem(QSL("wgt"), QSL("19999"));
                    } else if (novelLengthMode == NovelSearchLengthMode::nslmTime) {
                        m_sourceQuery.addQueryItem(QSL("rlt"), QSL("10"));
                        m_sourceQuery.addQueryItem(QSL("rgt"), QSL("59"));
                    } else {
                        m_sourceQuery.addQueryItem(QSL("tlt"), QSL("5000"));
                        m_sourceQuery.addQueryItem(QSL("tgt"), QSL("19999"));
                    }
                    break;
                case NovelSearchLength::nslMedium:
                    if (novelLengthMode == NovelSearchLengthMode::nslmWords) {
                        m_sourceQuery.addQueryItem(QSL("wlt"), QSL("0"));
                        m_sourceQuery.addQueryItem(QSL("wgt"), QSL("4999"));
                    } else if (novelLengthMode == NovelSearchLengthMode::nslmTime) {
                        m_sourceQuery.addQueryItem(QSL("rlt"), QSL("60"));
                        m_sourceQuery.addQueryItem(QSL("rgt"), QSL("179"));
                    } else {
                        m_sourceQuery.addQueryItem(QSL("tlt"), QSL("20000"));
                        m_sourceQuery.addQueryItem(QSL("tgt"), QSL("79999"));
                    }
                    break;
                case NovelSearchLength::nslLong:
                    if (novelLengthMode == NovelSearchLengthMode::nslmWords) {
                        m_sourceQuery.addQueryItem(QSL("wlt"), QSL("80000"));
                    } else if (novelLengthMode == NovelSearchLengthMode::nslmTime) {
                        m_sourceQuery.addQueryItem(QSL("rlt"), QSL("180"));
                    } else {
                        m_sourceQuery.addQueryItem(QSL("tlt"), QSL("80000"));
                    }
                    break;
            }
            if (originalOnly)
                m_sourceQuery.addQueryItem(QSL("original_only"), QSL("1"));
        }
        if (!languageCode.isEmpty()) {
            QString lang = languageCode.toLower();
            if (lang == QSL("zh"))
                lang = QSL("zh-cn");
            m_sourceQuery.addQueryItem(QSL("work_lang"),lang);
        }
        if (hideAIWorks)
            m_sourceQuery.addQueryItem(QSL("ai_type"), QSL("1"));
    }
}

QString CPixivIndexExtractor::workerDescription() const
{
    return tr("Pixiv index extractor (ID: %1)").arg(m_indexId);
}

/*
 * Get novel JSON data from IDs.
 */
void CPixivIndexExtractor::fetchNovelsInfo()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (exitIfAborted()) return;
    if (rpl) {
        if (rpl->error() == QNetworkReply::NoError) {
            QJsonParseError err {};
            const QByteArray data = rpl->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data,&err);
            addLoadedRequest(data.size());
            if (doc.isNull()) {
                showError(tr("JSON parser error %1 at %2.")
                          .arg(err.error)
                          .arg(err.offset));
                return;
            }

            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.value(QSL("error")).toBool(false)) {
                    showError(tr("Novel list extractor error: %1")
                              .arg(obj.value(QSL("message")).toString()));
                    return;
                }

                const QJsonObject tworks = obj.value(QSL("body")).toObject()
                                           .value(QSL("works")).toObject();

                for (const auto& work : qAsConst(tworks)) {
                    const QJsonObject w = work.toObject();

                    const QDateTime createDT = QDateTime::fromString(w.value(QSL("createDate")).toString(),
                                                                     Qt::ISODate);
                    // results ordered by date desc
                    if (!m_dateTo.isNull() && (createDT.date() > m_dateTo)) continue;
                    if (!m_dateFrom.isNull() && (createDT.date() < m_dateFrom)) {
                        m_ids.clear();
                        showIndexResult(rpl->url());
                        return;
                    }

                    m_list.append(w);

                    if ((m_maxCount > 0) && (m_list.count() >= m_maxCount)) {
                        m_ids.clear();
                        showIndexResult(rpl->url());
                        return;
                    }
                }
            }
        }
    }

    if (!rpl.isNull() && (m_maxCount > 0) && (m_list.count() >= m_maxCount)) {
        m_ids.clear();
        showIndexResult(rpl->url());
        return;
    }

    // All IDs parsed? Go to postprocessing.
    if (m_ids.isEmpty()) {
        if (rpl) {
            showIndexResult(rpl->url());
        } else {
            showError(tr("Novel list is empty."));
        }
        return;
    }

    // Parse next IDs group.
    const QString key = QSL("ids%5B%5D");
    const int maxQueryLen = 1024;

    QUrlQuery uq;
    int len = 0;
    while ((len < maxQueryLen) && !m_ids.isEmpty()) {
        QString v = m_ids.takeFirst();
        uq.addQueryItem(key,v);
        len += key.length()+v.length()+2;
    }
    QUrl url(QSL("https://www.pixiv.net/ajax/user/%1/profile/novels")
             .arg(m_indexId));
    url.setQuery(uq);

    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url]{
        if (exitIfAborted()) return;
        QNetworkRequest req(url);
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivIndexExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::fetchNovelsInfo);
    },Qt::QueuedConnection);
}

/*
 * Get artwork JSON data from IDs.
 */
void CPixivIndexExtractor::fetchArtworksInfo()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (exitIfAborted()) return;
    if (rpl) {
        if (rpl->error() == QNetworkReply::NoError) {
            QJsonParseError err {};
            const QByteArray data = rpl->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data,&err);
            addLoadedRequest(data.size());
            if (doc.isNull()) {
                showError(tr("JSON parser error %1 at %2.")
                          .arg(err.error)
                          .arg(err.offset));
                return;
            }

            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.value(QSL("error")).toBool(false)) {
                    showError(tr("Novel list extractor error: %1")
                              .arg(obj.value(QSL("message")).toString()));
                    return;
                }

                const QJsonObject tworks = obj.value(QSL("body")).toObject()
                                           .value(QSL("works")).toObject();

                for (const auto& work : qAsConst(tworks)) {
                    const QJsonObject w = work.toObject();

                    // value illustType == 1 for manga, 0 for illust

                    const QDateTime createDT = QDateTime::fromString(w.value(QSL("createDate")).toString(),
                                                                     Qt::ISODate);
                    // results ordered by date desc
                    if (!m_dateTo.isNull() && (createDT.date() > m_dateTo)) continue;
                    if (!m_dateFrom.isNull() && (createDT.date() < m_dateFrom)) {
                        m_illustsIds.clear();
                        m_mangaIds.clear();
                        showIndexResult(rpl->url());
                        return;
                    }

                    m_list.append(w);

                    // maxCount limiter
                    if ((m_maxCount > 0) && (m_list.count() >= m_maxCount)) {
                        m_illustsIds.clear();
                        m_mangaIds.clear();
                        showIndexResult(rpl->url());
                        return;
                    }
                }
            }
        }
    }

    // maxCount limiter
    if (!rpl.isNull() && (m_maxCount > 0) && (m_list.count() >= m_maxCount)) {
        m_illustsIds.clear();
        m_mangaIds.clear();
        showIndexResult(rpl->url());
        return;
    }

    // Parse next IDs group.
    const QString key = QSL("ids%5B%5D");
    const int maxQueryLen = 1024;

    QUrlQuery uq;
    QUrl url(QSL("https://www.pixiv.net/ajax/user/%1/profile/illusts")
             .arg(m_indexId));

    if (!m_mangaIds.isEmpty()) {
        int len = 0;
        while (len < maxQueryLen && !m_mangaIds.isEmpty()) {
            QString v = m_mangaIds.takeFirst();
            uq.addQueryItem(key,v);
            len += key.length()+v.length()+2;
        }
        uq.addQueryItem(QSL("work_category"),QSL("manga"));
        uq.addQueryItem(QSL("is_first_page"),(rpl.isNull() ? QSL("1") : QSL("0")));
        url.setQuery(uq);

    } else if (!m_illustsIds.isEmpty()) {
        int len = 0;
        while (len < maxQueryLen && !m_illustsIds.isEmpty()) {
            QString v = m_illustsIds.takeFirst();
            uq.addQueryItem(key,v);
            len += key.length()+v.length()+2;
        }
        uq.addQueryItem(QSL("work_category"),QSL("illust"));
        uq.addQueryItem(QSL("is_first_page"),(rpl.isNull() ? QSL("1") : QSL("0")));
        url.setQuery(uq);

    } else { // All IDs parsed? Go to postprocessing.
        if (rpl) {
            showIndexResult(rpl->url());
        } else {
            showError(tr("Artworks list is empty."));
        }
        return;
    }

    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url]{
        if (exitIfAborted()) return;
        QNetworkRequest req(url);
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivIndexExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::fetchArtworksInfo);
    },Qt::QueuedConnection);
}


void CPixivIndexExtractor::startMain()
{
    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
        m_ids.clear();
        m_illustsIds.clear();
        m_mangaIds.clear();
        m_list = QJsonArray();
        if (exitIfAborted()) return;

        QUrl u;
        QString querySelector;
        switch (m_indexMode) {
            case imWorkIndex:
                u = QUrl(QSL("https://www.pixiv.net/ajax/user/%1/profile/all").arg(m_indexId));
                break;
            case imBookmarksIndex:
                switch (m_extractorMode) {
                    case emNovels: querySelector = QSL("novels"); break;
                    case emArtworks: querySelector = QSL("illusts"); break;
                }
                u = QUrl(QSL("https://www.pixiv.net/ajax/user/%1/%2/bookmarks?"
                             "tag=&offset=0&limit=%3&rest=show")
                         .arg(m_indexId,querySelector)
                         .arg(CDefaults::pixivBookmarksFetchCount));
                break;
            case imTagSearchIndex:
                switch (m_extractorMode) {
                    case emNovels: querySelector = QSL("novels"); break;
                    case emArtworks: querySelector = QSL("artworks"); break;
                }
                u = QUrl(QSL("https://www.pixiv.net/ajax/search/%1/%2")
                         .arg(querySelector,m_indexId));
                u.setQuery(m_sourceQuery);
                break;
        }

        QNetworkRequest req(u);
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivIndexExtractor::loadError);
        switch (m_indexMode) {
            case imWorkIndex:
                connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::profileAjax);
                break;
            case imBookmarksIndex:
                connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::bookmarksAjax);
                break;
            case imTagSearchIndex:
                connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::searchAjax);
                break;
        }
    },Qt::QueuedConnection);
}

/*
 * Get all novel IDs from pixiv author profile.
 */
void CPixivIndexExtractor::profileAjax()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;
    if (exitIfAborted()) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QJsonParseError err {};
        const QByteArray data = rpl->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data,&err);
        addLoadedRequest(data.size());
        if (doc.isNull()) {
            showError(tr("JSON parser error %1 at %2.")
                      .arg(err.error)
                      .arg(err.offset));
            return;
        }

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.value(QSL("error")).toBool(false)) {
                showError(tr("Novel list extractor error: %1")
                          .arg(obj.value(QSL("message")).toString()));
                return;
            }

            if (m_extractorMode == emNovels) {
                m_ids = obj.value(QSL("body")).toObject()
                        .value(QSL("novels")).toObject().keys();

                QMetaObject::invokeMethod(this,[this]{
                    fetchNovelsInfo();
                },Qt::QueuedConnection);
            } else {
                m_illustsIds = obj.value(QSL("body")).toObject()
                               .value(QSL("illusts")).toObject().keys();
                m_mangaIds = obj.value(QSL("body")).toObject()
                             .value(QSL("manga")).toObject().keys();

                QMetaObject::invokeMethod(this,[this]{
                    fetchArtworksInfo();
                },Qt::QueuedConnection);

            }
        }
    }
}

/*
 * Get all novel JSON data from pixiv author bookmarks.
 */
void CPixivIndexExtractor::bookmarksAjax()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;
    if (exitIfAborted()) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QUrlQuery uq(rpl->url());
        bool ok = false;
        int offset = uq.queryItemValue(QSL("offset")).toInt(&ok);
        if (!ok)
            offset = 0;

        QJsonParseError err {};
        const QByteArray data = rpl->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data,&err);
        addLoadedRequest(data.size());
        if (doc.isNull()) {
            showError(tr("JSON parser error %1 at %2.")
                      .arg(err.error)
                      .arg(err.offset));
            return;
        }

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.value(QSL("error")).toBool(false)) {
                showError(tr("Novel list extractor error: %1")
                          .arg(obj.value(QSL("message")).toString()));
                return;
            }

            const QJsonArray tworks = obj.value(QSL("body")).toObject()
                                      .value(QSL("works")).toArray();

            for (const auto& work : qAsConst(tworks)) {
                const QJsonObject w = work.toObject();

                const QDateTime createDT = QDateTime::fromString(w.value(QSL("createDate")).toString(),
                                                                 Qt::ISODate);
                // bookmarks is not ordered by date, simple filter all results
                if (!m_dateFrom.isNull() && (createDT.date() < m_dateFrom)) continue;
                if (!m_dateTo.isNull() && (createDT.date() > m_dateTo)) continue;

                m_list.append(w);

                if ((m_maxCount > 0) && (m_list.count() >= m_maxCount)) {
                    showIndexResult(rpl->url());
                    return;
                }
            }

            int totalWorks = obj.value(QSL("body")).toObject()
                             .value(QSL("total")).toInt();

            if ((totalWorks>m_list.count()) && (tworks.count()>0)) {
                // We still have unfetched links, and last fetch was not empty
                QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,offset]{
                    if (exitIfAborted()) return;
                    QString querySelector;
                    switch (m_extractorMode) {
                        case emNovels: querySelector = QSL("novels"); break;
                        case emArtworks: querySelector = QSL("illusts"); break;
                    }
                    QUrl u(QSL("https://www.pixiv.net/ajax/user/%1/%2/bookmarks?"
                                          "tag=&offset=%3&limit=%4&rest=show")
                           .arg(m_indexId,querySelector)
                           .arg(offset+CDefaults::pixivBookmarksFetchCount)
                           .arg(CDefaults::pixivBookmarksFetchCount));
                    QNetworkRequest req(u);
                    QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

                    connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivIndexExtractor::loadError);
                    connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::bookmarksAjax);
                },Qt::QueuedConnection);
                return;
            }

            showIndexResult(rpl->url());
            return;
        }
    }
    Q_EMIT finished();
}

/*
 * Get all novel JSON data from pixiv tag search.
 */
void CPixivIndexExtractor::searchAjax()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;
    if (exitIfAborted()) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QUrlQuery uq(rpl->url());
        bool ok = false;
        int page = uq.queryItemValue(QSL("p")).toInt(&ok);
        if (!ok)
            page = 1;

        QJsonParseError err {};
        const QByteArray data = rpl->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data,&err);
        addLoadedRequest(data.size());
        if (doc.isNull()) {
            showError(tr("JSON parser error %1 at %2.")
                      .arg(err.error)
                      .arg(err.offset));
            return;
        }

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.value(QSL("error")).toBool(false)) {
                showError(tr("Novel list extractor error: %1")
                          .arg(obj.value(QSL("message")).toString()));
                return;
            }

            QString selector;
            switch (m_extractorMode) {
                case emNovels: selector = QSL("novel"); break;
                case emArtworks: selector = QSL("illustManga"); break;
            }
            const QJsonArray tworks = obj.value(QSL("body")).toObject()
                                      .value(selector).toObject()
                                      .value(QSL("data")).toArray();

            for (const auto& work : tworks) {
                m_list.append(work.toObject());

                if ((m_maxCount > 0) && (m_list.count() >= m_maxCount)) {
                    showIndexResult(rpl->url());
                    return;
                }
            }

            int totalWorks = obj.value(QSL("body")).toObject()
                             .value(selector).toObject()
                             .value(QSL("total")).toInt();

            if ((totalWorks>m_list.count()) && (tworks.count()>0)) {
                // We still have unfetched links, and last fetch was not empty
                QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,page]{
                    if (exitIfAborted()) return;
                    QString querySelector;
                    switch (m_extractorMode) {
                        case emNovels: querySelector = QSL("novels"); break;
                        case emArtworks: querySelector = QSL("artworks"); break;
                    }
                    QUrl u(QSL("https://www.pixiv.net/ajax/search/%1/%2")
                           .arg(querySelector,m_indexId));
                    QUrlQuery uq = m_sourceQuery;
                    uq.addQueryItem(QSL("p"),QSL("%1").arg(page+1));
                    u.setQuery(uq);
                    QNetworkRequest req(u);
                    QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

                    connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivIndexExtractor::loadError);
                    connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::searchAjax);
                },Qt::QueuedConnection);
                return;
            }

            showIndexResult(rpl->url());
            return;
        }
    }
    Q_EMIT finished();
}

/*
 * JSON list postprocessing. Get cover images and transfer to results table.
 */
void CPixivIndexExtractor::showIndexResult(const QUrl &origin)
{
    preloadNovelCovers(origin);
    if (m_worksImgFetch>0) return;

    const QJsonArray list = m_list;
    auto *window = gSet->activeWindow();
    auto indexMode = m_indexMode;
    auto indexID = m_indexId;
    auto sourceQuery = m_sourceQuery;
    auto extractorMode = m_extractorMode;

    QString filterDesc;
    if (m_maxCount>0)
        filterDesc = tr("%1 max count").arg(m_maxCount);
    if (!m_dateFrom.isNull()) {
        if (!filterDesc.isEmpty()) filterDesc.append(QSL(", "));
        filterDesc.append(tr("from %1").arg(QLocale::system().toString(m_dateFrom,QLocale::ShortFormat)));
    }
    if (!m_dateTo.isNull()) {
        if (!filterDesc.isEmpty()) filterDesc.append(QSL(", "));
        filterDesc.append(tr("to %1").arg(QLocale::system().toString(m_dateTo,QLocale::ShortFormat)));
    }
    if (filterDesc.isEmpty())
        filterDesc = tr("None");

    QMetaObject::invokeMethod(window,[origin,list,window,indexMode,indexID,sourceQuery,filterDesc,extractorMode](){
        new CPixivIndexTab(window,list,extractorMode,indexMode,indexID,sourceQuery,filterDesc,origin);
    },Qt::QueuedConnection);

    Q_EMIT finished();
}

void CPixivIndexExtractor::preloadNovelCovers(const QUrl& origin)
{
    static const QByteArray emptyImage =
            QByteArrayLiteral("data:image/gif;base64,R0lGODlhAQABAAD/ACwAAAAAAQABAAACADs=");
    const QStringList &supportedExt = CGenericFuncs::getSupportedImageExtensions();

    if ((m_extractorMode == emArtworks) ||
            (gSet->settings()->pixivFetchCovers == CStructures::pxfmLazyFetch)) {
        m_worksImgFetch.storeRelease(0);
        return;
    }

    int imgWorkCounter = 0;
    QStringList processedUrls;
    m_imgMutex.lock();
    for (int i=0; i<m_list.count(); i++) {
        QJsonObject obj = m_list.at(i).toObject();
        QString coverImgUrl = obj.value(QSL("url")).toString();
        if (coverImgUrl.contains(QSL("data:image"))) continue;

        if (gSet->settings()->pixivFetchCovers == CStructures::pxfmNone) {
            obj.insert(QSL("url"),QJsonValue(QString::fromUtf8(emptyImage)));
            m_list[i] = obj;
            continue;
        }

        if (coverImgUrl.contains(QSL("common/images"))) {
            const QString data = gSet->net()->getPixivCommonCover(coverImgUrl);
            if (!data.isEmpty()) {
                obj.insert(QSL("url"),QJsonValue(data));
                m_list[i] = obj;
                continue;
            }
        }

        QUrl url(coverImgUrl);
        if (!url.isValid() || url.isEmpty()) continue;

        QFileInfo fi(coverImgUrl);
        if (supportedExt.contains(fi.suffix(),Qt::CaseInsensitive) &&
                !processedUrls.contains(coverImgUrl)) {
            imgWorkCounter++;
            processedUrls.append(coverImgUrl);
            QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,origin]{
                if (exitIfAborted()) return;
                QNetworkRequest req(url);
                req.setRawHeader("referer",origin.toString().toUtf8());
                QNetworkReply *rplImg = gSet->net()->auxNetworkAccessManagerGet(req);
                connect(rplImg,&QNetworkReply::finished,this,&CPixivIndexExtractor::subImageFinished);
            },Qt::QueuedConnection);
        }
    }
    m_worksImgFetch.storeRelease(imgWorkCounter);
    m_imgMutex.unlock();
}

void CPixivIndexExtractor::subImageFinished()
{
    static const QByteArray errorImage =
            QByteArrayLiteral("data:image/gif;base64,R0lGODlhAQABAAD/ACwAAAAAAQABAAACADs=");

    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull() || exitIfAborted()) {
        if (m_list.isEmpty()) {
            Q_EMIT auxImgLoadFinished(QUrl(),QString());
        }
        return;
    }

    QByteArray dataUri;
    if (rpl->error() == QNetworkReply::NoError) {
        QByteArray ba = rpl->readAll();
        addLoadedRequest(ba.size());
        QImage img;
        if (img.loadFromData(ba)) {
            if (img.width()>img.height()) {
                if (img.width()>gSet->settings()->pdfImageMaxSize) {
                    img = img.scaledToWidth(gSet->settings()->pdfImageMaxSize,
                                            Qt::SmoothTransformation);
                }
            } else {
                if (img.height()>gSet->settings()->pdfImageMaxSize) {
                    img = img.scaledToHeight(gSet->settings()->pdfImageMaxSize,
                                             Qt::SmoothTransformation);
                }
            }
            ba.clear();
            QBuffer buf(&ba);
            buf.open(QIODevice::WriteOnly);
            if (img.save(&buf,"JPEG",gSet->settings()->pdfImageQuality)) {
                dataUri = QByteArrayLiteral("data:image/jpeg;base64,");
                dataUri.append(ba.toBase64());
            }
        }
    }

    if (dataUri.isEmpty())
        dataUri = errorImage;

    const QUrl origin = rpl->url();
    const QString data = QString::fromUtf8(dataUri);
    const QString rplUrl = origin.toString();

    if (rplUrl.contains(QSL("common/images")))
        gSet->net()->addPixivCommonCover(rplUrl,data);

    if (m_list.isEmpty()) {
        // Auxiliary call
        Q_EMIT auxImgLoadFinished(origin,data);
        return;
    }

    m_imgMutex.lock();
    for (int i=0; i<m_list.count(); i++) {
        QJsonObject obj = m_list.at(i).toObject();
        const QString coverImgUrl = obj.value(QSL("url")).toString();
        if (coverImgUrl == rplUrl) {
            obj.insert(QSL("url"),QJsonValue(data));
            m_list[i] = obj;
        }
    }
    m_imgMutex.unlock();

    QMetaObject::invokeMethod(this,[this,origin](){
        showIndexResult(origin);
    },Qt::QueuedConnection);
    m_worksImgFetch--;
}

bool CPixivIndexExtractor::extractorLimitsDialog(QWidget *parentWidget,
                                                 CPixivIndexExtractor::ExtractorMode exMode,
                                                 bool isTagSearch,
                                                 int &maxCount, QDate &dateFrom, QDate &dateTo, QString &keywords,
                                                 CPixivIndexExtractor::TagSearchMode &tagMode, bool &originalOnly,
                                                 QString &languageCode,
                                                 CPixivIndexExtractor::NovelSearchLength &novelLength,
                                                 CPixivIndexExtractor::SearchRating &novelRating,
                                                 CPixivIndexExtractor::ArtworkSearchType &artworkType,
                                                 CPixivIndexExtractor::ArtworkSearchSize &artworkSize,
                                                 CPixivIndexExtractor::ArtworkSearchRatio &artworkRatio,
                                                 QString &artworkCreationTool,
                                                 bool &hideAIWorks,
                                                 CPixivIndexExtractor::NovelSearchLengthMode &novelLengthMode)
{
    CPixivIndexLimitsDialog dlg(parentWidget);
    auto fetchCovers = gSet->settings()->pixivFetchCovers;
    dlg.setParams(exMode,isTagSearch,maxCount,dateFrom,dateTo,keywords,tagMode,
                  originalOnly,fetchCovers,languageCode,novelLength,novelRating,artworkType,
                  artworkSize,artworkRatio,artworkCreationTool,hideAIWorks,novelLengthMode);

    if (dlg.exec() == QDialog::Accepted) {
        dlg.getParams(maxCount,dateFrom,dateTo,keywords,tagMode,originalOnly,
                      fetchCovers,languageCode,novelLength,novelRating,artworkType,
                      artworkSize,artworkRatio,artworkCreationTool,hideAIWorks,novelLengthMode);
        gSet->ui()->setPixivFetchCovers(fetchCovers);
        return true;
    }
    return false;
}
