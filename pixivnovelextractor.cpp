#include <QApplication>
#include <QString>
#include <QMessageBox>
#include <QUrlQuery>
#include <QFileInfo>
#include <QBuffer>
#include <QImage>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QRegularExpression>

#include "pixivnovelextractor.h"
#include "genericfuncs.h"
#include "globalcontrol.h"

CPixivNovelExtractor::CPixivNovelExtractor(QObject *parent)
    : QObject(parent)
{

}

void CPixivNovelExtractor::setParams(CSnippetViewer *viewer, const QUrl &source, const QString &title,
                                     bool translate, bool focus)
{
    m_snv = viewer;
    m_title = title;
    m_translate = translate;
    m_focus = focus;
    m_source = source;
}

void CPixivNovelExtractor::setMangaOrigin(CSnippetViewer *viewer, const QUrl &origin)
{
    m_snv = viewer;
    m_mangaOrigin = origin;
}

void CPixivNovelExtractor::start()
{
    if (!m_source.isValid()) {
        Q_EMIT finished();
        return;
    }

    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
        QNetworkRequest req(m_source);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivNovelExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::novelLoadFinished);
    },Qt::QueuedConnection);
}

void CPixivNovelExtractor::startManga()
{
    if (!m_mangaOrigin.isValid()) {
        Q_EMIT finished();
        return;
    }

    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
        QNetworkRequest req(m_mangaOrigin);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);
        req.setMaximumRedirectsAllowed(CDefaults::httpMaxRedirects);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivNovelExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
    },Qt::QueuedConnection);
}

void CPixivNovelExtractor::novelLoadFinished()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull() || m_snv==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QString html = QString::fromUtf8(rpl->readAll());

        QUrl origin = rpl->url();
        QUrlQuery qr(origin);

        m_novelId = qr.queryItemValue(QSL("id"));

        QString wtitle = m_title;
        if (wtitle.isEmpty())
            wtitle = CGenericFuncs::extractFileTitle(html);

        QString hauthor;
        QString hauthornum;
        QString htitle;
        QStringList tags;

        QRegularExpression rxToken(QSL("\\{\\s*\\\"token\\\"\\s*:"));
        int idx = html.indexOf(rxToken);
        if (idx>0) {
            html.remove(0,idx);
            idx = html.indexOf(QSL("</script>"));
            if (idx>=0)
                html.truncate(idx);
            idx = html.indexOf(QSL("}\">"));
            if (idx>=0)
                html.truncate(idx+1);
            idx = html.lastIndexOf(QSL("})"));
            if (idx>0)
                html.truncate(idx+1);

            html = parseJsonNovel(html,tags,hauthor,hauthornum,htitle);
        } else {
            html = tr("Unable to extract novel. Unknown page structure.");
        }

        QRegularExpression rbrx(QSL("\\[\\[rb\\:.*?\\]\\]"));
        int pos = 0;
        QRegularExpressionMatch match = rbrx.match(html,pos);
        while (match.hasMatch()) {
            QString rb = match.captured();
            rb.replace(QSL("&gt;"), QSL(">"));
            rb.remove(QRegularExpression(QSL("^.*rb\\:")));
            rb.remove(QRegularExpression(QSL("\\>.*")));
            rb = rb.trimmed();
            if (!rb.isEmpty()) {
                html.replace(match.capturedStart(),match.capturedLength(),rb);
            } else {
                pos += match.capturedLength();
            }
            match = rbrx.match(html,pos);
        }

        QRegularExpression imrx(QSL("\\[pixivimage:\\S+\\]"));
        QStringList imgs;
        auto it = imrx.globalMatch(html);
        while (it.hasNext()) {
            match = it.next();
            QString im = match.captured();
            im.remove(QRegularExpression(QSL(".*:")));
            im.remove(']');
            im = im.trimmed();
            if (!im.isEmpty())
                imgs << im;
        }

        m_title = wtitle;
        m_origin = origin;
        handleImages(imgs);

        if (!tags.isEmpty()) {
            QString tagList;
            for (const auto& tag : qAsConst(tags)) {
                if (!tagList.isEmpty())
                    tagList.append(QSL(" / "));
                tagList.append(QSL("<a href=\"https://www.pixiv.net/tags/%1/novels\">%1</a>").arg(tag));
            }
            if (!tagList.isEmpty())
                html.prepend(QSL("Tags: %1\n\n").arg(tagList));
        }
        if (!hauthor.isEmpty()) {
            html.prepend(QSL("Author: <a href=\"https://www.pixiv.net/users/%1\">%2</a>\n\n")
                         .arg(hauthornum,hauthor));
        }
        if (!htitle.isEmpty())
            html.prepend(QSL("Title: <b>%1</b>\n\n").arg(htitle));


        m_html = html;

        subWorkFinished(); // call just in case for empty lists
    }
}

void CPixivNovelExtractor::loadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    QString msg(QSL("Unable to load from pixiv."));

    auto *rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl)
        msg.append(QSL(" %1").arg(rpl->errorString()));

    qCritical() << msg;

    QWidget *w = nullptr;
    QObject *ctx = QApplication::instance();
    if (m_snv) {
        w = m_snv->parentWnd();
        ctx = m_snv;
    }
    QMetaObject::invokeMethod(ctx,[msg,w](){
        QMessageBox::warning(w,tr("JPReader"),msg);
    },Qt::QueuedConnection);
    Q_EMIT finished();
}

void CPixivNovelExtractor::subLoadFinished()
{
    const QStringList &supportedExt = CGenericFuncs::getSupportedImageExtensions();

    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;

    QUrl rplUrl = rpl->url();
    QString key = rplUrl.fileName();

    int httpStatus = -1;
    QVariant vstat = rpl->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (vstat.isValid()) {
        bool ok = false;
        httpStatus = vstat.toInt(&ok);
    }

    if ((rpl->error() == QNetworkReply::NoError) && (httpStatus<CDefaults::httpCodeRedirect)) {
        // valid page without redirect or error
        const CIntList idxs = m_imgList.value(key);
        QString html = QString::fromUtf8(rpl->readAll());

        QString id;
        QVector<CUrlWithName> imageUrls = parseJsonIllustPage(html,rplUrl,&id);

        // Aux manga load from context menu
        if (m_mangaOrigin.isValid()) {
            Q_EMIT mangaReady(imageUrls,id,m_mangaOrigin);
            Q_EMIT finished();
            return;
        }

        m_imgMutex.lock();
        for (auto it = idxs.constBegin(), end = idxs.constEnd(); it != end; ++it) {
            int page = (*it);
            if (page<1 || page>imageUrls.count()) continue;
            QUrl url = QUrl(imageUrls.at(page-1).first);
            m_imgUrls[QSL("%1_%2").arg(key).arg(page)] = url.toString();

            QFileInfo fi(url.toString());
            if (gSet->settings()->pixivFetchImages &&
                    supportedExt.contains(fi.suffix(),Qt::CaseInsensitive)) {
                m_worksImgFetch++;
                QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,rplUrl]{
                    QNetworkRequest req(url);
                    req.setRawHeader("referer",rplUrl.toString().toUtf8());
                    QNetworkReply *rplImg = gSet->auxNetworkAccessManager()->get(req);
                    connect(rplImg,&QNetworkReply::finished,this,&CPixivNovelExtractor::subImageFinished);
                },Qt::QueuedConnection);
            }
        }
        m_imgMutex.unlock();
    } else {
        qWarning() << "Failed to fetch image from " << rplUrl;
        qDebug() << rpl->rawHeaderPairs();
    }
    QMetaObject::invokeMethod(this,&CPixivNovelExtractor::subWorkFinished,Qt::QueuedConnection);
    m_worksPageLoad--;
}

void CPixivNovelExtractor::subWorkFinished()
{
    if (m_worksPageLoad>0 || m_worksImgFetch>0) return;

    // replace fetched image urls
    for (auto it = m_imgUrls.constBegin(), end = m_imgUrls.constEnd(); it != end; ++it) {
        QStringList kl = it.key().split('_');

        QRegularExpression rx;
        if (kl.last()==QSL("1")) {
            rx = QRegularExpression(QSL("\\[pixivimage:%1(?:-1)?\\]").arg(kl.first()));
        } else {
            rx = QRegularExpression(QSL("\\[pixivimage:%1-%2\\]")
                         .arg(kl.first(),kl.last()));
        }

        QString rpl = QSL("<br /><img src=\"%1\"").arg(it.value());
        if (it.value().startsWith(QSL("data:"))) {
            rpl.append(QSL("/ ><br />"));
        } else {
            rpl.append(QSL(" width=\"%1px;\"/ ><br />")
                       .arg(gSet->settings()->pdfImageMaxSize));
        }
        m_html.replace(rx, rpl);
    }

    Q_EMIT novelReady(CGenericFuncs::makeSimpleHtml(m_title,m_html,true,m_origin),m_focus,m_translate);
    Q_EMIT finished();
}

void CPixivNovelExtractor::subImageFinished()
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
        for (auto it = m_imgUrls.keyValueBegin(), end = m_imgUrls.keyValueEnd();
             it != end; ++it) {
            if (((*it).second==rpl->url().toString()) && !ba.isEmpty())
                (*it).second = QString::fromUtf8(ba);
        }
        m_imgMutex.unlock();
    }
    QMetaObject::invokeMethod(this,&CPixivNovelExtractor::subWorkFinished,Qt::QueuedConnection);
    m_worksImgFetch--;
}

void CPixivNovelExtractor::handleImages(const QStringList &imgs)
{
    m_imgList.clear();
    m_imgUrls.clear();

    for(const QString &img : imgs) {
        if (img.indexOf('-')>0) {
            QStringList sl = img.split('-');
            bool ok = false;
            int page = sl.last().toInt(&ok);
            if (ok && page>0)
                m_imgList[sl.first()].append(page);
        } else
            m_imgList[img].append(1);
    }

    m_worksPageLoad = m_imgList.count();
    m_worksImgFetch = 0;
    for(auto it  = m_imgList.constBegin(), end = m_imgList.constEnd(); it != end; ++it) {
        QUrl url(QSL("https://www.pixiv.net/artworks/%1").arg(it.key()));
        QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url]{
            QNetworkRequest req(url);
            req.setRawHeader("referer",m_origin.toString().toUtf8());
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);
            req.setMaximumRedirectsAllowed(CDefaults::httpMaxRedirects);
            QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);
            connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
        },Qt::QueuedConnection);
    }
}

QJsonDocument CPixivNovelExtractor::parseJsonSubDocument(const QByteArray& source, const QRegularExpression& start)
{
    QJsonDocument doc;

    QString src = QString::fromUtf8(source);
    QRegularExpressionMatch match;
    int idx = src.indexOf(start,0,&match);
    if (idx<0) {
        doc = QJsonDocument::fromJson(R"({"error":"Unable to find JSON sub-document."})");
        return doc;
    }
    QByteArray cnt = src.mid(idx+match.capturedLength()-1).toUtf8();

    QJsonParseError err {};
    doc = QJsonDocument::fromJson(cnt,&err);
    if (doc.isNull()) {
        if (err.error == QJsonParseError::GarbageAtEnd) {
            cnt.truncate(err.offset);
        } else {
            const QString s = QSL(R"({"error":"JSON parser error %1 at %2."})")
                              .arg(err.error)
                              .arg(err.offset);
            doc = QJsonDocument::fromJson(s.toUtf8());
            return doc;
        }
    }
    // try again
    doc = QJsonDocument::fromJson(cnt,&err);
    if (doc.isNull()) {
        const QString s = QSL(R"({"error":"JSON reparser error %1 at %2."})")
                          .arg(err.error)
                          .arg(err.offset);
        doc = QJsonDocument::fromJson(s.toUtf8());
        return doc;
    }

    return doc;
}

QVector<CUrlWithName> CPixivNovelExtractor::parseJsonIllustPage(const QString &html, const QUrl &origin, QString* id)
{
    QVector<CUrlWithName> res;
    QJsonDocument doc;

    QString key = origin.fileName();
    if (id != nullptr)
        *id = key;

    QRegularExpression jstart(QSL("\\s*\\\"illust\\\"\\s*:\\s*{\\s*\\\"%1\\\"\\s*:\\s*{").arg(key));
    if (html.indexOf(jstart)>=0) {
        doc = parseJsonSubDocument(html.toUtf8(),jstart);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();

            QString err = obj.value(QSL("error")).toString();
            if (!err.isNull()) {
                qWarning() << QSL("Images extractor error: %1").arg(err);
                return res;
            }
        }
    } else {
        return res;
    }

    QString illustId;
    QString suffix;
    QString path;
    int pageCount = -1;

    if (doc.isObject()) {
        QJsonObject obj = doc.object();

        QString err = obj.value(QSL("error")).toString();
        if (!err.isNull()) {
            qWarning() << QSL("Images extractor error: %1").arg(err);
            return res;
        }

        illustId = obj.value(QSL("illustId")).toString();
        pageCount = qRound(obj.value(QSL("pageCount")).toDouble(-1.0));

        QString fname = obj.value(QSL("urls")).toObject()
                        .value(QSL("original")).toString();
        if (!fname.isNull()) {
            QFileInfo fi(fname);
            path = fi.path();
            suffix = fi.completeSuffix();
        }
    }

    for (int i=0;i<pageCount;i++) {
        if (path.isEmpty() || illustId.isEmpty() || suffix.isEmpty())
            continue;

        res.append(qMakePair(QSL("%1/%2_p%3.%4")
                             .arg(path,illustId,QString::number(i),suffix),
                             QString()));
    }

    return res;
}

QString CPixivNovelExtractor::parseJsonNovel(const QString &html, QStringList &tags,
                                             QString &author, QString &authorNum,
                                             QString &title)
{
    QByteArray cnt = html.toUtf8();
    QString res;

    QRegularExpression rxNovel(QSL("\\s*\\\"%1\\\"\\s*:\\s*{").arg(m_novelId));
    QJsonDocument doc = parseJsonSubDocument(cnt,rxNovel);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();

        QString err = obj.value(QSL("error")).toString();
        if (!err.isNull())
            return QSL("Novel extractor error: %1").arg(err);

        res = obj.value(QSL("content")).toString();
        title = obj.value(QSL("title")).toString();
        authorNum = obj.value(QSL("userId")).toString();

        const QJsonArray vtags = obj.value(QSL("tags")).toObject()
                                 .value(QSL("tags")).toArray();
        for(const auto &tag : vtags) {
            QString t = tag.toObject().value(QSL("tag")).toString();
            if (!t.isEmpty())
                tags.append(t);
        }

    } else {
        return tr("ERROR: Unable to find novel subdocument.");

    }

    QRegularExpression rxAuthor(QSL("\\s*\\\"%1\\\"\\s*:\\s*{").arg(authorNum));
    doc = parseJsonSubDocument(cnt,rxAuthor);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();

        QString err = obj.value(QSL("error")).toString();
        if (!err.isNull())
            return QSL("Author parser error: %1").arg(err);

        author = obj.value(QSL("name")).toString();

    } else {
        return tr("ERROR: Unable to find author subdocument.");

    }

    return res;
}
