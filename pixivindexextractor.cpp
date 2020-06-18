#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>

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

                for (const auto& work : qAsConst(tworks)) {
                    const auto w = work.toObject();

                    QStringList tags;
                    const QJsonArray wtags = w.value(QSL("tags")).toArray();
                    tags.reserve(wtags.count());
                    for (const auto& t : qAsConst(wtags))
                        tags.append(t.toString());

                    addNovelInfoBlock(w.value(QSL("id")).toString(),
                                      w.value(QSL("url")).toString(),
                                      w.value(QSL("title")).toString(),
                                      w.value(QSL("textCount")).toInt(),
                                      w.value(QSL("userName")).toString(),
                                      w.value(QSL("userId")).toString(),
                                      tags,
                                      w.value(QSL("description")).toString());
                }
            }
        }
    }

    if (m_ids.isEmpty()) {
        finalizeHtml();
        Q_EMIT finished();
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
        m_html.clear();
        m_infoBlockCount = 0;
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
        m_html.clear();
        m_infoBlockCount = 0;
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

            for (const auto& work : qAsConst(tworks)) {
                const auto w = work.toObject();

                QStringList tags;
                const QJsonArray wtags = w.value(QSL("tags")).toArray();
                tags.reserve(wtags.count());
                for (const auto& t : qAsConst(wtags))
                    tags.append(t.toString());

                addNovelInfoBlock(w.value(QSL("id")).toString(),
                                  w.value(QSL("url")).toString(),
                                  w.value(QSL("title")).toString(),
                                  w.value(QSL("textCount")).toInt(),
                                  w.value(QSL("userName")).toString(),
                                  w.value(QSL("userId")).toString(),
                                  tags,
                                  w.value(QSL("description")).toString());
            }

            int totalWorks = obj.value(QSL("body")).toObject()
                             .value(QSL("total")).toInt();

            if ((totalWorks>m_infoBlockCount) && (tworks.count()>0)) {
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

            finalizeHtml();
        }
    }
    Q_EMIT finished();
}

void CPixivIndexExtractor::addNovelInfoBlock(const QString& workId, const QString& workImgUrl,
                                             const QString& title, int length,
                                             const QString& author, const QString& authorId,
                                             const QStringList& tags, const QString& description)
{
    m_html.append(QSL("<div style='border-top:1px black solid;padding-top:10px;overflow:auto;'>"));
    m_html.append(QSL("<div style='width:120px;float:left;margin-right:20px;'>"));
    m_html.append(QSL("<a href=\"https://www.pixiv.net/novel/show.php?"
                                 "id=%1\"><img src=\"%2\" style='width:120px;'/></a>")
                  .arg(workId,workImgUrl));
    m_html.append(QSL("</div><div style='float:none;'>"));

    m_html.append(QSL("<b>Title:</b> <a href=\"https://www.pixiv.net/novel/show.php?"
                                 "id=%1\">%2</a>.<br/>")
                  .arg(workId,title));

    m_html.append(QSL("<b>Size:</b> %1 characters.<br/>")
                  .arg(length));

    m_html.append(QSL("<b>Author:</b> <a href=\"https://www.pixiv.net/member.php?"
                                 "id=%1\">%2</a>.<br/>")
                  .arg(authorId,author));

    if (!tags.isEmpty()) {
        m_html.append(QSL("<b>Tags:</b> %1.<br/>")
                      .arg(tags.join(QSL(" / "))));
    }

    m_html.append(QSL("<b>Description:</b> %1.<br/>")
                  .arg(description));

    m_html.append(QSL("</div></div>\n"));

    m_authors[authorId] = author;
    m_infoBlockCount++;
}

void CPixivIndexExtractor::finalizeHtml()
{
    QString header;
    switch (m_indexMode) {
        case WorkIndex: header = tr("works"); break;
        case BookmarksIndex: header = tr("bookmarks"); break;
        default: break;
    }
    QString author = m_authors.value(m_authorId);
    if (author.isEmpty())
        author = tr("author");

    QString title = tr("Pixiv %1 list for %2").arg(header,author);
    header = QSL("<h3>Pixiv %1 list for <a href=\"https://www.pixiv.net/member.php?"
                            "id=%2\">%3</a>.</h3>").arg(header,m_authorId,author);

    if (m_html.isEmpty()) {
        m_html = tr("Nothing found.");
    } else {
        header.append(tr("Found %1 novels.").arg(m_infoBlockCount));
    }
    m_html.prepend(header);
    m_html = CGenericFuncs::makeSimpleHtml(title,m_html);

    Q_EMIT listReady(m_html);
}
