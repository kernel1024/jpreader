#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

#include "kemonoextractor.h"

#include "global/control.h"
#include "global/network.h"
#include "utils/genericfuncs.h"

namespace CDefaults {
const int maxKemonoFilenameLength = 180;
}

CKemonoExtractor::CKemonoExtractor(QObject *parent)
    : CAbstractExtractor{parent}
{}

void CKemonoExtractor::setParams(const QString &sourceRepo, int authorId, int postId, bool translate,
                                 bool alternateTranslate, bool focus,
                                 bool isManga, bool download, const CStringHash &auxInfo)
{
    m_translate = translate;
    m_alternateTranslate = alternateTranslate;
    m_focus = focus;
    m_authorId = authorId;
    m_sourceRepo = sourceRepo;
    m_postId = postId;
    m_isManga = isManga;
    m_download = download;
    m_auxInfo = auxInfo;
}

QString CKemonoExtractor::workerDescription() const
{
    return tr("Kemono extractor (post: %1)").arg(m_postId);
}

void CKemonoExtractor::startMain()
{
    static QHash<int,QString> authorIndex;

    auto postFetcher = [this]{
        if (exitIfAborted()) return;
        QNetworkRequest req(QUrl(QSL("https://kemono.cr/api/v1/%1/user/%2/post/%3").arg(m_sourceRepo).arg(m_authorId).arg(m_postId)));
        QByteArray ref = QSL("https://kemono.cr/%1/user/%2/post/%3").arg(m_sourceRepo).arg(m_authorId).arg(m_postId).toUtf8();
        req.setRawHeader("referer",ref);
        req.setRawHeader("Accept","text/css");
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CKemonoExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CKemonoExtractor::pageLoadFinished);
    };

    auto authorFetcher = [this,postFetcher]{
        if (exitIfAborted()) return;
        QNetworkRequest req(QUrl(QSL("https://kemono.cr/api/v1/%1/user/%2/profile").arg(m_sourceRepo).arg(m_authorId)));
        QByteArray ref = QSL("https://kemono.cr/%1/user/%2/post/%3").arg(m_sourceRepo).arg(m_authorId).arg(m_postId).toUtf8();
        req.setRawHeader("referer",ref);
        req.setRawHeader("Accept","text/css");
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CKemonoExtractor::loadError);

        connect(rpl,&QNetworkReply::finished,this,[this,rpl,postFetcher]{
            QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> reply(rpl);
            if (reply.isNull()) return;
            if (exitIfAborted()) return;
            if (reply->error() == QNetworkReply::NoError) {
                QUrl origin = reply->url();
                QJsonParseError err {};
                const QByteArray data = reply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data,&err);
                addLoadedRequest(data.size());

                if (!doc.isNull() && doc.isObject()) {
                    m_authorsMutex.lock();
                    bool ok = false;
                    int authorId = doc.object().value(QSL("id")).toString().toInt(&ok);
                    if (ok) {
                        QString authorName = doc.object().value(QSL("name")).toString();
                        authorIndex.insert(authorId,authorName);
                        m_author = authorName;
                    }
                    m_authorsMutex.unlock();
                }
            }

            QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),postFetcher,Qt::QueuedConnection);




        });
    };

    if ((m_postId<=0) || (m_authorId<=0) || m_sourceRepo.isEmpty()) {
        Q_EMIT finished();
        return;
    }

    m_authorsMutex.lock();
    m_author = authorIndex.value(m_authorId);
    m_authorsMutex.unlock();

    if (!m_author.isEmpty()) {
        QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),postFetcher,Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),authorFetcher,Qt::QueuedConnection);
    }
}

void CKemonoExtractor::pageLoadFinished()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;
    if (exitIfAborted()) return;
    if (rpl->error() == QNetworkReply::NoError) {
        const QUrl origin = rpl->url();
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
            QJsonObject post = doc.object().value(QSL("post")).toObject();
            if (post.isEmpty()) {
                showError(tr("JSON post is empty."));
                return;
            }

            m_postNum = post.value(QSL("id")).toString();
            m_title = post.value(QSL("title")).toString();
            m_text = post.value(QSL("content")).toString();

            QStringList tags;
            const QJsonArray jtags = post.value(QSL("tags")).toArray();
            tags.reserve(jtags.count());
            for (const auto &jtag : jtags) {
                QString tag = jtag.toString();
                if (!tag.isEmpty())
                    tags.append(tag);
            }

            QVector<CUrlWithName> images;
            const QJsonArray jimgPreviews = doc.object().value(QSL("previews")).toArray();
            images.reserve(jimgPreviews.count());
            for (const auto& img: jimgPreviews) {
                const QString server = img.toObject().value(QSL("server")).toString();
                const QString relUrl = img.toObject().value(QSL("path")).toString();
                const QString fileName = img.toObject().value(QSL("name")).toString();
                QString url = QSL("%1/data%2").arg(server,relUrl);
                if (!fileName.isEmpty()) {
                    url = QSL("%1?f=%2").arg(url,fileName);
                }
                if (!url.isEmpty())
                    images.append(qMakePair(url,fileName));
            }

            if (!tags.isEmpty()) {
                QString tagList;
                for (const auto& tag : std::as_const(tags)) {
                    if (!tagList.isEmpty())
                        tagList.append(QSL(" / "));
                    tagList.append(QSL("<a href=\"https://kemono.cr/%1/user/%2?tag=%3\">%3</a>")
                                       .arg(m_sourceRepo).arg(m_authorId).arg(tag));
                }
                if (!tagList.isEmpty())
                    m_text.prepend(QSL("Tags: %1\n\n").arg(tagList));
            }
            if (!m_auxInfo.value("author").isEmpty()) {
                m_author = m_auxInfo.value("author");
            }
            if (!m_author.isEmpty()) {
                m_text.prepend(QSL("Author: <a href=\"https://kemono.cr/%1/user/%2\">%3</a>\n\n")
                                   .arg(m_sourceRepo).arg(m_authorId).arg(m_author));
            }

            if (!m_isManga) {
                CStringHash info = m_auxInfo;
                info.insert(QSL("title"), m_title);
                info.insert(QSL("id"), QSL("%1").arg(m_postNum));
                Q_EMIT novelReady(CGenericFuncs::makeSimpleHtml(
                                      m_title,m_text,true,
                                      QUrl(QSL("https://kemono.cr/%1/user/%2/post/%3").arg(m_sourceRepo).arg(m_authorId).arg(m_postId))),
                                  m_focus,m_translate,m_alternateTranslate,m_download,info);
            }

            if (m_isManga && !images.isEmpty()) {
                QString mangaId = m_postNum;
                if (!m_title.isEmpty()) {
                    mangaId = CGenericFuncs::makeSafeFilename(
                        QSL("%1 [kemono_%2]")
                            .arg(CGenericFuncs::elideString(
                                     m_title,
                                     CDefaults::maxKemonoFilenameLength),
                                 m_postNum));
                }
                if (!m_author.isEmpty())
                    mangaId.prepend(QSL("[%1] ").arg(m_author));

                QString description;
                for (const auto& tag : std::as_const(tags)) {
                    if (description.isEmpty()) {
                        description.append(QSL("<b>Tags:</b> "));
                    } else {
                        description.append(QSL(" / "));
                    }
                    description.append(tag);
                }
                if (!description.isEmpty())
                    description.append(QSL("<br/>"));
                if (!m_author.isEmpty())
                    description.append(QSL("<b>Author:</b> %1").arg(m_author));

                Q_EMIT mangaReady(images,mangaId,QSL("https://kemono.cr/%1/user/%2/post/%3").arg(m_sourceRepo).arg(m_authorId).arg(m_postId),
                                  m_title,description,!m_download,m_focus,true);
            }
        }
    }

    Q_EMIT finished();
}
