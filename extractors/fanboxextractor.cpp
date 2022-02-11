#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include "fanboxextractor.h"
#include "global/control.h"
#include "global/network.h"
#include "utils/genericfuncs.h"

namespace CDefaults {
const int maxFanboxFilenameLength = 180;
}

CFanboxExtractor::CFanboxExtractor(QObject *parent)
    : CAbstractExtractor(parent)
{
}

void CFanboxExtractor::setParams(int postId, bool translate, bool alternateTranslate, bool focus, bool isManga)
{
    m_translate = translate;
    m_alternateTranslate = alternateTranslate;
    m_focus = focus;
    m_postId = postId;
    m_isManga = isManga;
}

QString CFanboxExtractor::workerDescription() const
{
    return tr("Fanbox extractor (post: %1)").arg(m_postId);
}

void CFanboxExtractor::startMain()
{
    if (m_postId<=0) {
        Q_EMIT finished();
        return;
    }

    m_illustMap.clear();
    m_worksIllustFetch = 0;

    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
        if (exitIfAborted()) return;
        QNetworkRequest req(QUrl(QSL("https://api.fanbox.cc/post.info?postId=%1").arg(m_postId)));
        req.setRawHeader("referer","https://www.fanbox.cc/");
        req.setRawHeader("accept","application/json, text/plain, */*");
        req.setRawHeader("origin","https://www.fanbox.cc");
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CFanboxExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CFanboxExtractor::pageLoadFinished);
    },Qt::QueuedConnection);
}

void CFanboxExtractor::pageLoadFinished()
{
    const QStringList &supportedExt = CGenericFuncs::getSupportedImageExtensions();

    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;
    if (exitIfAborted()) return;
    if (rpl->error() == QNetworkReply::NoError) {
        QUrl origin = rpl->url();
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
            QJsonObject body = doc.object().value(QSL("body")).toObject();
            if (body.isEmpty()) {
                showError(tr("JSON body is empty."));
                return;
            }
            QJsonObject mbody = body.value(QSL("body")).toObject();
            if (mbody.isEmpty()) {
                showError(tr("JSON post body is empty."));
                return;
            }

            m_postNum = body.value(QSL("id")).toString();
            m_title = body.value(QSL("title")).toString();
            QString author = body.value(QSL("user")).toObject().value(QSL("name")).toString();
            m_authorId = body.value(QSL("creatorId")).toString();
            m_text = mbody.value(QSL("text")).toString();
            QStringList imageIdOrder;
            const QJsonArray jblocks = mbody.value(QSL("blocks")).toArray();
            imageIdOrder.reserve(jblocks.count());
            for (const auto &jblock : jblocks) {
                QString bvalue = jblock.toObject().value(QSL("type")).toString();
                QString btext = jblock.toObject().value(QSL("text")).toString();
                QString imageId = jblock.toObject().value(QSL("imageId")).toString();
                if (!imageId.isEmpty()) {
                    m_text.append(QSL("<img>%1</img>").arg(imageId));
                    imageIdOrder.append(imageId);
                } else if (!bvalue.isEmpty()) {
                    m_text.append(QSL("<%1>%2</%1>").arg(bvalue,btext));
                }
            }

            QStringList tags;
            const QJsonArray jtags = body.value(QSL("tags")).toArray();
            tags.reserve(jtags.count());
            for (const auto &jtag : jtags) {
                QString tag = jtag.toString();
                if (!tag.isEmpty())
                    tags.append(tag);
            }

            QVector<CUrlWithName> images;
            const QJsonArray jimgUrls = mbody.value(QSL("images")).toArray();
            images.reserve(jimgUrls.count());
            for (const auto &jimg : jimgUrls) {
                QString url = jimg.toObject().value(QSL("originalUrl")).toString();
                if (!url.isEmpty())
                    images.append(qMakePair(url,QString()));
            }

            if (!tags.isEmpty()) {
                QString tagList;
                for (const auto& tag : qAsConst(tags)) {
                    if (!tagList.isEmpty())
                        tagList.append(QSL(" / "));
                    tagList.append(QSL("<a href=\"https://%1.fanbox.cc/tags/%2\">%2</a>")
                                   .arg(m_authorId,tag));
                }
                if (!tagList.isEmpty())
                    m_text.prepend(QSL("Tags: %1\n\n").arg(tagList));
            }
            if (!author.isEmpty()) {
                m_text.prepend(QSL("Author: <a href=\"https://%1.fanbox.cc\">%2</a>\n\n")
                             .arg(m_authorId,author));
            }

            QHash<QString,QString> imageIdHash;
            const QJsonObject jillusts = mbody.value(QSL("imageMap")).toObject();
            for (const auto &jimg : jillusts) {
                QString id = jimg.toObject().value(QSL("id")).toString();
                QString url = jimg.toObject().value(QSL("originalUrl")).toString();
                if (!id.isEmpty() && !url.isEmpty()) {
                    m_illustMap.append(qMakePair(url,id));
                    imageIdHash.insert(id,url);
                }
            }

            if (!m_isManga && !m_illustMap.isEmpty()) {
                QStringList processedUrls;
                m_illustMutex.lock();
                for (const auto& w : qAsConst(m_illustMap)) {
                    QString illustUrl = w.first;
                    QUrl url(illustUrl);
                    if (!url.isValid() || url.isEmpty()) continue;

                    QFileInfo fi(illustUrl);
                    if (gSet->settings()->pixivFetchImages &&
                            supportedExt.contains(fi.suffix(),Qt::CaseInsensitive) &&
                            !processedUrls.contains(illustUrl)) {
                        m_worksIllustFetch++;
                        processedUrls.append(illustUrl);
                        QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,origin]{
                            if (exitIfAborted()) return;
                            QNetworkRequest req(url);
                            req.setRawHeader("origin","https://www.fanbox.cc");
                            req.setRawHeader("referer",origin.toString().toUtf8());
                            QNetworkReply *rplImg = gSet->net()->auxNetworkAccessManagerGet(req);
                            connect(rplImg,&QNetworkReply::finished,this,&CFanboxExtractor::subImageFinished);
                        },Qt::QueuedConnection);
                    }
                }
                m_illustMutex.unlock();

            } else {
                if (!m_isManga) {
                    Q_EMIT novelReady(CGenericFuncs::makeSimpleHtml(
                                          m_title,m_text,true,
                                          QUrl(QSL("http://%1.fanbox.cc/posts/%2").arg(m_authorId,m_postNum))),
                                      m_focus,m_translate,m_alternateTranslate);
                }

                if (m_isManga && images.isEmpty() && !imageIdHash.isEmpty()) {
                    for (const auto &ord : qAsConst(imageIdOrder)) {
                        if (imageIdHash.contains(ord))
                            images.append(qMakePair(imageIdHash.take(ord),QString()));
                    }
                    for (auto it = imageIdHash.constKeyValueBegin(), end = imageIdHash.constKeyValueEnd(); it != end; ++it)
                        images.append(qMakePair((*it).second,QString()));
                }

                if (m_isManga && !images.isEmpty()) {
                    QString mangaId = m_postNum;
                    if (!m_title.isEmpty()) {
                        mangaId = CGenericFuncs::makeSafeFilename(
                                    QSL("%1 [fanbox_%2]")
                                    .arg(CGenericFuncs::elideString(
                                             m_title,
                                             CDefaults::maxFanboxFilenameLength),
                                         m_postNum));
                    }
                    if (!m_authorId.isEmpty())
                        mangaId.prepend(QSL("[%1] ").arg(m_authorId));
                    Q_EMIT mangaReady(images,mangaId,QSL("https://www.fanbox.cc/@%1/posts/%2")
                                      .arg(m_authorId,m_postNum),false,false,true);
                }
            }
        }
    }

    if (m_worksIllustFetch == 0)
        Q_EMIT finished();
}

void CFanboxExtractor::subImageFinished()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;
    if (exitIfAborted()) return;
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
                QByteArray out("data:image/jpeg;base64,");
                out.append(ba.toBase64());
                ba = out;
            } else {
                ba.clear();
            }
        } else {
            ba.clear();
        }

        m_illustMutex.lock();
        for (auto &w : m_illustMap) {
            if ((w.first == rpl->url().toString()) && !ba.isEmpty()) {
                m_text.replace(QSL("<img>%1</img>").arg(w.second),
                               QSL("<img src=\"%1\"/>").arg(QString::fromUtf8(ba)));
            }
        }
        m_illustMutex.unlock();
    }
    m_worksIllustFetch--;

    if (m_worksIllustFetch == 0) {
        Q_EMIT novelReady(CGenericFuncs::makeSimpleHtml(
                              m_title,m_text,true,
                              QUrl(QSL("http://%1.fanbox.cc/posts/%2").arg(m_authorId,m_postNum))),
                          m_focus,m_translate,m_alternateTranslate);

        Q_EMIT finished();
    }
}
