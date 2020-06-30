#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QBuffer>
#include <algorithm>

#include "globalcontrol.h"
#include "genericfuncs.h"
#include "pixivindexextractor.h"

namespace CDefaults {
const int pixivBookmarksFetchCount = 24;
}

CPixivIndexExtractor::CPixivIndexExtractor(QObject *parent)
    : QObject(parent)
{

}

void CPixivIndexExtractor::setParams(CSnippetViewer *viewer, const QString &pixivId)
{
    m_snv = viewer;
    m_authorId = pixivId;
}

void CPixivIndexExtractor::showError(const QString &message)
{
    qCritical() << "CPixivIndexExtractor error:" << message;

    QWidget *w = nullptr;
    QObject *ctx = QApplication::instance();
    if (m_snv) {
        w = m_snv->parentWnd();
        ctx = m_snv;
    }
    QMetaObject::invokeMethod(ctx,[message,w](){
        QMessageBox::warning(w,tr("JPReader"),tr("CPixivIndexExtractor error:\n%1").arg(message));
    },Qt::QueuedConnection);
    Q_EMIT finished();
}

void CPixivIndexExtractor::fetchNovelsInfo()
{
    if (m_snv==nullptr) return;

    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl) {
        if (rpl->error() == QNetworkReply::NoError) {
            QJsonParseError err {};
            QJsonDocument doc = QJsonDocument::fromJson(rpl->readAll(),&err);
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

                m_list.reserve(tworks.count());
                for (const auto& work : qAsConst(tworks))
                    m_list.append(work.toObject());
            }
        }
    }

    if (m_ids.isEmpty()) {
        finalizeHtml(rpl->url());
        return;
    }

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
             .arg(m_authorId));
    url.setQuery(uq);

    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url]{
        QNetworkRequest req(url);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivIndexExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::fetchNovelsInfo);
    },Qt::QueuedConnection);
}

void CPixivIndexExtractor::loadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    QString msg(QSL("Unable to load from pixiv."));

    auto *rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl)
        msg.append(QSL(" %1").arg(rpl->errorString()));

    showError(msg);
}

void CPixivIndexExtractor::createNovelList()
{
    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
        m_ids.clear();
        m_list.clear();
        m_indexMode = WorkIndex;

        QUrl u(QSL("https://www.pixiv.net/ajax/user/%1/profile/all").arg(m_authorId));
        QNetworkRequest req(u);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivIndexExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::profileAjax);
    },Qt::QueuedConnection);
}

void CPixivIndexExtractor::createNovelBookmarksList()
{
    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
        m_ids.clear();
        m_list.clear();
        m_indexMode = BookmarksIndex;

        QUrl u(QSL("https://www.pixiv.net/ajax/user/%1/novels/bookmarks?"
                              "tag=&offset=0&limit=%2&rest=show")
               .arg(m_authorId)
               .arg(CDefaults::pixivBookmarksFetchCount));
        QNetworkRequest req(u);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivIndexExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::bookmarksAjax);
    },Qt::QueuedConnection);
}

void CPixivIndexExtractor::profileAjax()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull() || m_snv==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QJsonParseError err {};
        QJsonDocument doc = QJsonDocument::fromJson(rpl->readAll(),&err);
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

            m_ids = obj.value(QSL("body")).toObject()
                    .value(QSL("novels")).toObject().keys();

            QMetaObject::invokeMethod(this,[this]{
                fetchNovelsInfo();
            },Qt::QueuedConnection);
        }
    }
}

void CPixivIndexExtractor::bookmarksAjax()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull() || m_snv==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QUrlQuery uq(rpl->url());
        bool ok = false;
        int offset = uq.queryItemValue(QSL("offset")).toInt(&ok);
        if (!ok)
            offset = 0;

        QJsonParseError err {};
        QJsonDocument doc = QJsonDocument::fromJson(rpl->readAll(),&err);
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

            m_list.reserve(tworks.count());
            for (const auto& work : qAsConst(tworks))
                m_list.append(work.toObject());

            int totalWorks = obj.value(QSL("body")).toObject()
                             .value(QSL("total")).toInt();

            if ((totalWorks>m_list.count()) && (tworks.count()>0)) {
                // We still have unfetched links, and last fetch was not empty
                QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,offset]{
                    QUrl u(QSL("https://www.pixiv.net/ajax/user/%1/novels/bookmarks?"
                                          "tag=&offset=%2&limit=%3&rest=show")
                           .arg(m_authorId)
                           .arg(offset+CDefaults::pixivBookmarksFetchCount)
                           .arg(CDefaults::pixivBookmarksFetchCount));
                    QNetworkRequest req(u);
                    QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

                    connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivIndexExtractor::loadError);
                    connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::bookmarksAjax);
                },Qt::QueuedConnection);
                return;
            }

            finalizeHtml(rpl->url());
            return;
        }
    }
    Q_EMIT finished();
}

QString CPixivIndexExtractor::makeNovelInfoBlock(CStringHash *authors,
                                                 const QString& workId, const QString& workImgUrl,
                                                 const QString& title, int length,
                                                 const QString& author, const QString& authorId,
                                                 const QStringList& tags, const QString& description,
                                                 const QDateTime& creationDate, const QString& seriesTitle,
                                                 const QString& seriesId)
{
    QLocale locale;
    QString res;

    res.append(QSL("<div style='border-top:1px black solid;padding-top:10px;overflow:auto;'>"));
    res.append(QSL("<div style='width:120px;float:left;margin-right:20px;'>"));
    res.append(QSL("<a href=\"https://www.pixiv.net/novel/show.php?"
                                 "id=%1\"><img src=\"%2\" style='width:120px;'/></a>")
                  .arg(workId,workImgUrl));
    res.append(QSL("</div><div style='float:none;'>"));

    res.append(QSL("<b>Title:</b> <a href=\"https://www.pixiv.net/novel/show.php?"
                                 "id=%1\">%2</a>.<br/>")
                  .arg(workId,title));

    if (!seriesTitle.isEmpty() && !seriesId.isEmpty()) {
        res.append(QSL("<b>Series:</b> <a href=\"https://www.pixiv.net/novel/series/%1\">"
                          "%2</a>.<br/>")
                      .arg(seriesId,seriesTitle));
    }

    res.append(QSL("<b>Size:</b> %1 characters.<br/>")
                  .arg(length));

    if (creationDate.isValid()) {
        res.append(QSL("<b>Date:</b> %1.<br/>")
                      .arg(locale.toString(creationDate,QLocale::LongFormat)));
    }

    res.append(QSL("<b>Author:</b> <a href=\"https://www.pixiv.net/users/%1\">"
                                 "%2</a>.<br/>")
                  .arg(authorId,author));

    if (!tags.isEmpty()) {
        res.append(QSL("<b>Tags:</b> %1.<br/>")
                      .arg(tags.join(QSL(" / "))));
    }

    res.append(QSL("<b>Description:</b> %1.<br/>")
                  .arg(description));

    res.append(QSL("</div></div>\n"));

    (*authors)[authorId] = author;

    return res;
}

void CPixivIndexExtractor::finalizeHtml(const QUrl& origin)
{
    preloadNovelCovers(origin);
    if (m_worksImgFetch>0) return;

    QString html;
    CStringHash authors;

    if (m_list.isEmpty()) {
        html = tr("Nothing found.");

    } else {
        std::sort(m_list.rbegin(),m_list.rend(),[](const QJsonObject& c1, const QJsonObject& c2){
            const auto d1 = QDateTime::fromString(c1.value(QSL("createDate")).toString(),Qt::ISODate);
            const auto d2 = QDateTime::fromString(c2.value(QSL("createDate")).toString(),Qt::ISODate);
            return (d1<d2);
        });

        for (const auto &w : qAsConst(m_list)) {
            QStringList tags;
            const QJsonArray wtags = w.value(QSL("tags")).toArray();
            tags.reserve(wtags.count());
            for (const auto& t : qAsConst(wtags))
                tags.append(t.toString());

            html.append(makeNovelInfoBlock(&authors,
                                           w.value(QSL("id")).toString(),
                                           w.value(QSL("url")).toString(),
                                           w.value(QSL("title")).toString(),
                                           w.value(QSL("textCount")).toInt(),
                                           w.value(QSL("userName")).toString(),
                                           w.value(QSL("userId")).toString(),
                                           tags,
                                           w.value(QSL("description")).toString(),
                                           QDateTime::fromString(w.value(QSL("createDate")).toString(),
                                                                 Qt::ISODate),
                                           w.value(QSL("seriesTitle")).toString(),
                                           w.value(QSL("seriesId")).toString()));
        }
        html.append(tr("Found %1 novels.").arg(m_list.count()));
    }

    QString header;
    switch (m_indexMode) {
        case WorkIndex: header = tr("works"); break;
        case BookmarksIndex: header = tr("bookmarks"); break;
        default: break;
    }
    QString author = authors.value(m_authorId);
    if (author.isEmpty())
        author = tr("author");

    const QString title = tr("Pixiv %1 list for %2").arg(header,author);
    header = QSL("<h3>Pixiv %1 list for <a href=\"https://www.pixiv.net/users/%2\">"
                            "%3</a>.</h3>").arg(header,m_authorId,author);
    html.prepend(header);

    html = CGenericFuncs::makeSimpleHtml(title,html);

    Q_EMIT listReady(html);

    Q_EMIT finished();
}

void CPixivIndexExtractor::preloadNovelCovers(const QUrl& origin)
{
    const QStringList &supportedExt = CGenericFuncs::getSupportedImageExtensions();
    m_worksImgFetch = 0;

    QStringList processedUrls;
    m_imgMutex.lock();
    for (const auto& w : qAsConst(m_list)) {
        QString coverImgUrl = w.value(QSL("url")).toString();
        if (coverImgUrl.contains(QSL("common/images")) ||
                coverImgUrl.contains(QSL("data:image"))) {
            continue;
        }
        QUrl url(w.value(QSL("url")).toString());
        if (!url.isValid() || url.isEmpty()) continue;

        QFileInfo fi(coverImgUrl);
        if (gSet->settings()->pixivFetchImages &&
                supportedExt.contains(fi.suffix(),Qt::CaseInsensitive) &&
                !processedUrls.contains(coverImgUrl)) {
            m_worksImgFetch++;
            processedUrls.append(coverImgUrl);
            QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,origin]{
                QNetworkRequest req(url);
                req.setRawHeader("referer",origin.toString().toUtf8());
                QNetworkReply *rplImg = gSet->auxNetworkAccessManager()->get(req);
                connect(rplImg,&QNetworkReply::finished,this,&CPixivIndexExtractor::subImageFinished);
            },Qt::QueuedConnection);
        }
    }
    m_imgMutex.unlock();
}

void CPixivIndexExtractor::subImageFinished()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QByteArray ba = rpl->readAll();
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
                QByteArray out("data:image/jpeg;base64,");
                out.append(QString::fromLatin1(ba.toBase64()));
                ba = out;
            } else {
                ba.clear();
            }
        } else {
            ba.clear();
        }

        m_imgMutex.lock();
        for (auto &w : m_list) {
            if ((w.value(QSL("url")).toString() == rpl->url().toString()) && !ba.isEmpty())
                w.insert(QSL("url"),QJsonValue(QString::fromUtf8(ba)));
        }
        m_imgMutex.unlock();
    }

    QUrl origin = rpl->url();
    QMetaObject::invokeMethod(this,[this,origin](){
        finalizeHtml(origin);
    },Qt::QueuedConnection);
    m_worksImgFetch--;
}
