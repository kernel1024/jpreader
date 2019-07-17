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
    gSet->app()->restoreOverrideCursor();

    qCritical() << "CPixivIndexExtractor error:" << message;

    QWidget *w = nullptr;
    QObject *ctx = QApplication::instance();
    if (m_snv) {
        w = m_snv->parentWnd();
        ctx = m_snv;
    }
    QTimer::singleShot(0,ctx,[message,w](){
        QMessageBox::warning(w,tr("JPReader"),tr("CPixivIndexExtractor error:\n%1").arg(message));
    });
    Q_EMIT finished();
}

void CPixivIndexExtractor::fetchNovelsInfo()
{
    if (m_snv==nullptr) return;

    auto rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl != nullptr) {
        if (rpl->error() == QNetworkReply::NoError) {
            QJsonParseError err {};
            QJsonDocument doc = QJsonDocument::fromJson(rpl->readAll(),&err);
            if (doc.isNull()) {
                showError(tr("JSON parser error %1 at %2.")
                          .arg(err.error)
                          .arg(err.offset));
                rpl->deleteLater();
                return;
            }

            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.value(QStringLiteral("error")).toBool(false)) {
                    showError(tr("Novel list extractor error: %1")
                              .arg(obj.value(QStringLiteral("message")).toString()));
                    rpl->deleteLater();
                    return;
                }

                const QJsonObject tworks = obj.value(QStringLiteral("body")).toObject()
                                           .value(QStringLiteral("works")).toObject();

                for (const auto& work : qAsConst(tworks)) {
                    const auto w = work.toObject();

                    QStringList tags;
                    const QJsonArray wtags = w.value(QStringLiteral("tags")).toArray();
                    tags.reserve(wtags.count());
                    for (const auto& t : qAsConst(wtags))
                        tags.append(t.toString());

                    addNovelInfoBlock(w.value(QStringLiteral("id")).toString(),
                                      w.value(QStringLiteral("url")).toString(),
                                      w.value(QStringLiteral("title")).toString(),
                                      w.value(QStringLiteral("textCount")).toInt(),
                                      w.value(QStringLiteral("userName")).toString(),
                                      w.value(QStringLiteral("userId")).toString(),
                                      tags,
                                      w.value(QStringLiteral("description")).toString());
                }
            }
        }
        rpl->deleteLater();
    }

    if (m_ids.isEmpty()) {
        finalizeHtml();
        Q_EMIT finished();
        return;
    }

    const QString key = QStringLiteral("ids%5B%5D");
    const int maxQueryLen = 1024;

    QUrlQuery uq;
    int len = 0;
    while ((len < maxQueryLen) && !m_ids.isEmpty()) {
        QString v = m_ids.takeFirst();
        uq.addQueryItem(key,v);
        len += key.length()+v.length()+4; // &, = and %5B%5D
    }
    QUrl url(QStringLiteral("https://www.pixiv.net/ajax/user/%1/profile/novels")
             .arg(m_authorId));
    url.setQuery(uq);

    QTimer::singleShot(0,gSet->auxNetworkAccessManager(),[this,url]{
        QNetworkRequest req(url);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,qOverload<QNetworkReply::NetworkError>(&QNetworkReply::error),
                this,&CPixivIndexExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::fetchNovelsInfo);
    });
}

void CPixivIndexExtractor::loadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    QString msg(QStringLiteral("Unable to load from pixiv."));

    auto rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl)
        msg.append(QStringLiteral(" %1").arg(rpl->errorString()));

    showError(msg);
}

void CPixivIndexExtractor::createNovelList()
{
    QTimer::singleShot(0,gSet->auxNetworkAccessManager(),[this]{
        m_ids.clear();
        m_html.clear();
        m_infoBlockCount = 0;
        m_indexMode = WorkIndex;

        QUrl u(QStringLiteral("https://www.pixiv.net/ajax/user/%1/profile/all").arg(m_authorId));
        QNetworkRequest req(u);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,qOverload<QNetworkReply::NetworkError>(&QNetworkReply::error),
                this,&CPixivIndexExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::profileAjax);
    });
}

void CPixivIndexExtractor::createNovelBookmarksList()
{
    QTimer::singleShot(0,gSet->auxNetworkAccessManager(),[this]{
        m_ids.clear();
        m_html.clear();
        m_infoBlockCount = 0;
        m_indexMode = BookmarksIndex;

        QUrl u(QStringLiteral("https://www.pixiv.net/ajax/user/%1/novels/bookmarks?"
                              "tag=&offset=0&limit=65535&rest=show").arg(m_authorId));
        QNetworkRequest req(u);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,qOverload<QNetworkReply::NetworkError>(&QNetworkReply::error),
                this,&CPixivIndexExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivIndexExtractor::bookmarksAjax);
    });
}

void CPixivIndexExtractor::profileAjax()
{
    auto rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==nullptr || m_snv==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QJsonParseError err {};
        QJsonDocument doc = QJsonDocument::fromJson(rpl->readAll(),&err);
        if (doc.isNull()) {
            showError(tr("JSON parser error %1 at %2.")
                      .arg(err.error)
                      .arg(err.offset));
            rpl->deleteLater();
            return;
        }

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.value(QStringLiteral("error")).toBool(false)) {
                showError(tr("Novel list extractor error: %1")
                          .arg(obj.value(QStringLiteral("message")).toString()));
                rpl->deleteLater();
                return;
            }

            m_ids = obj.value(QStringLiteral("body")).toObject()
                    .value(QStringLiteral("novels")).toObject().keys();

            QTimer::singleShot(0,this,[this]{
                fetchNovelsInfo();
            });
        }
    }
    rpl->deleteLater();
}

void CPixivIndexExtractor::bookmarksAjax()
{
    auto rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==nullptr || m_snv==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QJsonParseError err {};
        QJsonDocument doc = QJsonDocument::fromJson(rpl->readAll(),&err);
        if (doc.isNull()) {
            showError(tr("JSON parser error %1 at %2.")
                      .arg(err.error)
                      .arg(err.offset));
            rpl->deleteLater();
            return;
        }

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.value(QStringLiteral("error")).toBool(false)) {
                showError(tr("Novel list extractor error: %1")
                          .arg(obj.value(QStringLiteral("message")).toString()));
                rpl->deleteLater();
                return;
            }

            const QJsonArray tworks = obj.value(QStringLiteral("body")).toObject()
                                      .value(QStringLiteral("works")).toArray();

            for (const auto& work : qAsConst(tworks)) {
                const auto w = work.toObject();

                QStringList tags;
                const QJsonArray wtags = w.value(QStringLiteral("tags")).toArray();
                tags.reserve(wtags.count());
                for (const auto& t : qAsConst(wtags))
                    tags.append(t.toString());

                addNovelInfoBlock(w.value(QStringLiteral("id")).toString(),
                                  w.value(QStringLiteral("url")).toString(),
                                  w.value(QStringLiteral("title")).toString(),
                                  w.value(QStringLiteral("textCount")).toInt(),
                                  w.value(QStringLiteral("userName")).toString(),
                                  w.value(QStringLiteral("userId")).toString(),
                                  tags,
                                  w.value(QStringLiteral("description")).toString());
            }

            finalizeHtml();
        }
    }
    Q_EMIT finished();
    rpl->deleteLater();
}

void CPixivIndexExtractor::addNovelInfoBlock(const QString& workId, const QString& workImgUrl,
                                             const QString& title, int length,
                                             const QString& author, const QString& authorId,
                                             const QStringList& tags, const QString& description)
{
    m_html.append(QStringLiteral("<div style='border-top:1px black solid;padding-top:10px;'>"));
    m_html.append(QStringLiteral("<div style='width:120px;float:left;margin-right:20px;'>"));
    m_html.append(QStringLiteral("<a href=\"https://www.pixiv.net/novel/show.php?"
                                 "id=%1\"><img src=\"%2\" style='width:120px;'/></a>")
                  .arg(workId,workImgUrl));
    m_html.append(QStringLiteral("</div><div style='float:none;'>"));

    m_html.append(QStringLiteral("<b>Title:</b> <a href=\"https://www.pixiv.net/novel/show.php?"
                                 "id=%1\">%2</a>.<br/>")
                  .arg(workId,title));

    m_html.append(QStringLiteral("<b>Size:</b> %1 characters.<br/>")
                  .arg(length));

    m_html.append(QStringLiteral("<b>Author:</b> <a href=\"https://www.pixiv.net/member.php?"
                                 "id=%1\">%2</a>.<br/>")
                  .arg(authorId,author));

    if (!tags.isEmpty()) {
        m_html.append(QStringLiteral("<b>Tags:</b> %1.<br/>")
                      .arg(tags.join(QStringLiteral(" / "))));
    }

    m_html.append(QStringLiteral("<b>Description:</b> %1.<br/>")
                  .arg(description));

    m_html.append(QStringLiteral("</div></div>\n"));

    m_authors[authorId] = author;
    m_infoBlockCount++;
}

void CPixivIndexExtractor::finalizeHtml()
{
    gSet->app()->restoreOverrideCursor();

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
    header = QStringLiteral("<h3>Pixiv %1 list for <a href=\"https://www.pixiv.net/member.php?"
                            "id=%2\">%3</a>.</h3>").arg(header,m_authorId,author);

    if (m_html.isEmpty())
        m_html = tr("Nothing found.");
    m_html.prepend(header);
    m_html = CGenericFuncs::makeSimpleHtml(title,m_html);

    Q_EMIT listReady(m_html);
}
