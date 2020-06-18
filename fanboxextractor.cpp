#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "fanboxextractor.h"
#include "globalcontrol.h"

namespace CDefaults {
const int maxFanboxFilenameLength = 180;
}

CFanboxExtractor::CFanboxExtractor(QObject *parent)
    : QObject(parent)
{
}

void CFanboxExtractor::setParams(CSnippetViewer *viewer, int postId, bool translate, bool focus)
{
    m_snv = viewer;
    m_translate = translate;
    m_focus = focus;
    m_postId = postId;
}

void CFanboxExtractor::start()
{
    if (m_postId<=0) {
        Q_EMIT finished();
        return;
    }

    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
        QNetworkRequest req(QUrl(QSL("https://api.fanbox.cc/post.info?postId=%1").arg(m_postId)));
        req.setRawHeader("referer","https://www.fanbox.cc/");
        req.setRawHeader("accept","application/json, text/plain, */*");
        req.setRawHeader("origin","https://www.fanbox.cc");
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CFanboxExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CFanboxExtractor::pageLoadFinished);
    },Qt::QueuedConnection);
}

void CFanboxExtractor::pageLoadFinished()
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

            QString postNum = body.value(QSL("id")).toString();
            QString title = body.value(QSL("title")).toString();
            QString author = body.value(QSL("user")).toObject().value(QSL("name")).toString();
            QString authorId = body.value(QSL("creatorId")).toString();
            QString text = mbody.value(QSL("text")).toString();
            const QJsonArray jblocks = mbody.value(QSL("blocks")).toArray();
            for (const auto &jblock : jblocks) {
                QString bvalue = jblock.toObject().value(QSL("type")).toString();
                QString btext = jblock.toObject().value(QSL("text")).toString();
                if (!bvalue.isEmpty())
                    text.append(QSL("<%1>%2</%1>").arg(bvalue,btext));
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
            QJsonArray jimgUrls = mbody.value(QSL("images")).toArray();
            for (const auto &jimg : qAsConst(jimgUrls)) {
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
                                   .arg(authorId,tag));
                }
                if (!tagList.isEmpty())
                    text.prepend(QSL("Tags: %1\n\n").arg(tagList));
            }
            if (!author.isEmpty()) {
                text.prepend(QSL("Author: <a href=\"https://%1.fanbox.cc\">%2</a>\n\n")
                             .arg(authorId,author));
            }

            Q_EMIT novelReady(CGenericFuncs::makeSimpleHtml(
                                  title,text,true,
                                  QUrl(QSL("http://%1.fanbox.cc/posts/%2").arg(authorId,postNum))),
                              m_focus,m_translate);

            if (!images.isEmpty()) {
                QString mangaId = postNum;
                if (!title.isEmpty()) {
                    mangaId = CGenericFuncs::makeSafeFilename(
                                  QSL("%1 [fanbox_%2]")
                                  .arg(CGenericFuncs::elideString(
                                           title,
                                           CDefaults::maxFanboxFilenameLength),
                                       postNum));
                }
                if (!authorId.isEmpty())
                    mangaId.prepend(QSL("[%1] ").arg(authorId));
                Q_EMIT mangaReady(images,mangaId,QSL("https://www.fanbox.cc/@%1/posts/%2")
                                  .arg(authorId,postNum));
            }
        }
    }

    Q_EMIT finished();
}

void CFanboxExtractor::loadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    QString msg(QSL("Unable to load from fanbox.cc."));

    auto *rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl)
        msg.append(QSL(" %1").arg(rpl->errorString()));

    showError(msg);
}

void CFanboxExtractor::showError(const QString &message)
{
    qCritical() << "CPixivIndexExtractor error:" << message;

    QWidget *w = nullptr;
    QObject *ctx = QApplication::instance();
    if (m_snv) {
        w = m_snv->parentWnd();
        ctx = m_snv;
    }
    QTimer::singleShot(0,ctx,[message,w](){
        QMessageBox::warning(w,tr("JPReader"),tr("CFanboxExtractor error:\n%1").arg(message));
    });
    Q_EMIT finished();
}
