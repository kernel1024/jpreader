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

#include "pixivnovelextractor.h"
#include "genericfuncs.h"
#include "globalcontrol.h"

// TODO: migrate from QRegExp to QRegularExpression

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

void CPixivNovelExtractor::start()
{
    if (!m_source.isValid()) {
        Q_EMIT finished();
        return;
    }

    QTimer::singleShot(0,gSet->auxNetworkAccessManager(),[this]{
        QNetworkRequest req(m_source);
        QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);

        connect(rpl,qOverload<QNetworkReply::NetworkError>(&QNetworkReply::error),
                this,&CPixivNovelExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::novelLoadFinished);
    });
}

void CPixivNovelExtractor::novelLoadFinished()
{
    auto rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==nullptr || m_snv==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QString html = QString::fromUtf8(rpl->readAll());

        QUrl origin = rpl->url();
        QUrlQuery qr(origin);

        QString hauthor;
        QString hauthornum;

        m_novelId = qr.queryItemValue(QSL("id"));

        QString wtitle = m_title;
        if (wtitle.isEmpty())
            wtitle = CGenericFuncs::extractFileTitle(html);

        QString htitle;
        QRegExp hrx(QSL("<h1[^>]*class=\"title\"[^>]*>"),Qt::CaseInsensitive);
        int hidx = hrx.indexIn(html);
        if (hidx>=0) {
            int hpos = hidx+hrx.matchedLength();
            int hlen = html.indexOf('<',hpos) - hpos;
            htitle = html.mid(hpos, hlen);
        }

        QRegExp arx(QSL("<a[^>]*class=\"user-name\"[^>]*>"),Qt::CaseInsensitive);
        int aidx = arx.indexIn(html);
        if (aidx>=0) {
            QString acap = arx.cap();
            int anpos = acap.indexOf(QSL("id="))+3;
            int anlen = acap.indexOf('\"',anpos) - anpos;
            hauthornum = acap.mid(anpos,anlen);

            int apos = aidx+arx.matchedLength();
            int alen = html.indexOf('<',apos) - apos;
            hauthor = html.mid(apos, alen);
        }

        QStringList tags;
        QRegExp trx(QSL("<li[^>]*class=\"tag\"[^>]*>"),Qt::CaseInsensitive);
        QRegExp ttrx(QSL("class=\"text\"[^>]*>"),Qt::CaseInsensitive);
        int tpos = 0;
        int tstop = html.indexOf(QSL("template-work-tags"),Qt::CaseInsensitive);
        int tidx = trx.indexIn(html,tpos);
        while (tidx>=0) {
            tpos = tidx + trx.matchedLength();
            int ttidx = ttrx.indexIn(html,tpos) + ttrx.matchedLength();
            int ttlen = html.indexOf('<',ttidx) - ttidx;
            if (ttidx>=0 && ttlen>=0 && ttidx<tstop) {
                tags << html.mid(ttidx,ttlen);
            }
            tidx = trx.indexIn(html,tpos);
        }

        QRegExp rx(QSL("<textarea[^>]*id=\"novel_text\"[^>]*>"),Qt::CaseInsensitive);
        int idx = rx.indexIn(html);
        if (idx<0) {
            // something wrong here - try pixiv novel JSON parser
            idx = html.indexOf(QSL("{token:"));
            if (idx<=0) {
                // something very wrong here
                html = tr("Unable to extract novel. Unknown page structure.");
            } else {
                html.remove(0,idx);
                idx = html.indexOf(QSL("</script>"));
                if (idx>=0)
                    html.truncate(idx);
                idx = html.lastIndexOf(QSL("})"));
                if (idx>0)
                    html.truncate(idx+1);

                html = parseJsonNovel(html,tags,hauthor,hauthornum,htitle);

            }
        } else {
            html.remove(0,idx+rx.matchedLength());
            idx = html.indexOf(QSL("</textarea>"),0,Qt::CaseInsensitive);
            if (idx>=0)
                html.truncate(idx);
        }

        QRegExp rbrx(QSL("\\[\\[rb\\:.*\\]\\]"));
        rbrx.setMinimal(true);
        int pos = 0;
        while ((pos = rbrx.indexIn(html,pos)) != -1) {
            QString rb = rbrx.cap(0);
            rb.replace(QSL("&gt;"), QSL(">"));
            rb.remove(QRegExp(QSL("^.*rb\\:")));
            rb.remove(QRegExp(QSL("\\>.*")));
            rb = rb.trimmed();
            if (!rb.isEmpty()) {
                html.replace(pos,rbrx.matchedLength(),rb);
            } else {
                pos += rbrx.matchedLength();
            }
        }

        QRegExp imrx(QSL("\\[pixivimage\\:\\S+\\]"));
        pos = 0;
        QStringList imgs;
        while ((pos = imrx.indexIn(html,pos)) != -1) {
            QString im = imrx.cap(0);
            im.remove(QRegExp(QSL(".*\\:")));
            im.remove(']');
            im = im.trimmed();
            if (!im.isEmpty())
                imgs << im;
            pos += imrx.matchedLength();
        }

        m_title = wtitle;
        m_origin = origin;
        handleImages(imgs);

        if (!tags.isEmpty()) {
            html.prepend(QSL("Tags: %1\n\n")
                         .arg(tags.join(QSL(" / "))));
        }
        if (!hauthor.isEmpty()) {
            html.prepend(QSL("Author: <a href=\"https://www.pixiv.net/member.php?"
                                        "id=%1\">%2</a>\n\n")
                         .arg(hauthornum,hauthor));
        }
        if (!htitle.isEmpty())
            html.prepend(QSL("Title: <b>%1</b>\n\n").arg(htitle));


        m_html = html;

        subWorkFinished(); // call just in case for empty lists
    }
    rpl->deleteLater();
}

void CPixivNovelExtractor::loadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)
    gSet->app()->restoreOverrideCursor();

    QString msg(QSL("Unable to load from pixiv."));

    auto rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl)
        msg.append(QSL(" %1").arg(rpl->errorString()));

    qCritical() << msg;

    QWidget *w = nullptr;
    QObject *ctx = QApplication::instance();
    if (m_snv) {
        w = m_snv->parentWnd();
        ctx = m_snv;
    }
    QTimer::singleShot(0,ctx,[msg,w](){
        QMessageBox::warning(w,tr("JPReader"),msg);
    });
    Q_EMIT finished();
}

void CPixivNovelExtractor::subLoadFinished()
{
    const QStringList &supportedExt = CGenericFuncs::getSupportedImageExtensions();

    auto rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==nullptr) return;

    QUrl rplUrl = rpl->url();
    QUrlQuery qr(rplUrl);
    QString key = qr.queryItemValue(QSL("illust_id"));
    QString mode = qr.queryItemValue(QSL("mode"));
    bool mediumMode = (QSL("medium").compare(mode,Qt::CaseInsensitive) == 0);

    int httpStatus = -1;
    QVariant vstat = rpl->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (vstat.isValid()) {
        bool ok;
        httpStatus = vstat.toInt(&ok);
    }

    if ((rpl->error() == QNetworkReply::NoError) && (httpStatus<CDefaults::httpCodeRedirect)) {
        // valid page without redirect or error
        const CIntList idxs = m_imgList.value(key);
        QString html = QString::fromUtf8(rpl->readAll());

        QStringList imageUrls = parseJsonIllustPage(html,rplUrl); // try JSON parser
        if (imageUrls.isEmpty()) { // no JSON found
            imageUrls = parseIllustPage(html,mediumMode);
        }

        m_imgMutex.lock();
        for (auto it = idxs.constBegin(), end = idxs.constEnd(); it != end; ++it) {
            int page = (*it);
            if (page<1 || page>imageUrls.count()) continue;
            QUrl url = QUrl(imageUrls.at(page-1));
            m_imgUrls[QSL("%1_%2").arg(key).arg(page)] = url.toString();

            QFileInfo fi(url.toString());
            if (gSet->settings()->pixivFetchImages &&
                    supportedExt.contains(fi.suffix(),Qt::CaseInsensitive)) {
                m_worksImgFetch++;
                QTimer::singleShot(0,gSet->auxNetworkAccessManager(),[this,url,rplUrl]{
                    QNetworkRequest req(url);
                    req.setRawHeader("referer",rplUrl.toString().toUtf8());
                    QNetworkReply *rplImg = gSet->auxNetworkAccessManager()->get(req);
                    connect(rplImg,&QNetworkReply::finished,this,&CPixivNovelExtractor::subImageFinished);
                });
            }
        }
        m_imgMutex.unlock();
    } else {
        if ((!mediumMode) &&
                // not found manga page
                ((httpStatus>=CDefaults::httpCodeClientError && httpStatus<CDefaults::httpCodeServerError) ||
                 // or redirected from manga page (new style)
                 (httpStatus>=CDefaults::httpCodeRedirect && httpStatus<CDefaults::httpCodeClientError)))

        {
            // try single page load
            QUrl url(QSL("https://www.pixiv.net/member_illust.php"));
            QUrlQuery qr;
            qr.addQueryItem(QSL("mode"),QSL("medium"));
            qr.addQueryItem(QSL("illust_id"),key);
            url.setQuery(qr);
            QTimer::singleShot(0,gSet->auxNetworkAccessManager(),[this,url]{
                QNetworkRequest req(url);
                req.setRawHeader("referer",m_origin.toString().toUtf8());
                QNetworkReply* nrpl = gSet->auxNetworkAccessManager()->get(req);
                connect(nrpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
            });
            rpl->deleteLater();
            return;
        }
    }
    QMetaObject::invokeMethod(this,&CPixivNovelExtractor::subWorkFinished,Qt::QueuedConnection);
    m_worksPageLoad--;
    rpl->deleteLater();
}

void CPixivNovelExtractor::subWorkFinished()
{
    if (m_worksPageLoad>0 || m_worksImgFetch>0) return;

    gSet->app()->restoreOverrideCursor();

    // replace fetched image urls
    for (auto it = m_imgUrls.constBegin(), end = m_imgUrls.constEnd(); it != end; ++it) {
        QStringList kl = it.key().split('_');

        QRegExp rx;
        if (kl.last()==QSL("1")) {
            rx = QRegExp(QSL("\\[pixivimage\\:%1(-1)?\\]").arg(kl.first()));
        } else {
            rx = QRegExp(QSL("\\[pixivimage\\:%1-%2\\]")
                         .arg(kl.first(),kl.last()));
        }

        QString rpl = QSL("<img src=\"%1\"").arg(it.value());
        if (it.value().startsWith(QSL("data:"))) {
            rpl.append(QSL("/ >"));
        } else {
            rpl.append(QSL(" width=\"%1px;\"/ >")
                       .arg(gSet->settings()->pdfImageMaxSize));
        }
        m_html.replace(rx, rpl);
    }

    Q_EMIT novelReady(CGenericFuncs::makeSimpleHtml(m_title,m_html,true,m_origin),m_focus,m_translate);
    Q_EMIT finished();
}

void CPixivNovelExtractor::subImageFinished()
{
    auto rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QByteArray ba = rpl->readAll();

        m_imgMutex.lock();
        for (auto it = m_imgUrls.keyValueBegin(), end = m_imgUrls.keyValueEnd();
             it != end; ++it) {
            if ((*it).second==rpl->url().toString()) {
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
                    img.save(&buf,"JPEG",gSet->settings()->pdfImageQuality);
                    
                    QByteArray out("data:image/jpeg;base64,");
                    out.append(QString::fromLatin1(ba.toBase64()));
                    
                    (*it).second = QString::fromUtf8(out);
                }
            }
        }
        m_imgMutex.unlock();
    }
    QMetaObject::invokeMethod(this,&CPixivNovelExtractor::subWorkFinished,Qt::QueuedConnection);
    m_worksImgFetch--;
    rpl->deleteLater();
}

void CPixivNovelExtractor::handleImages(const QStringList &imgs)
{
    m_imgList.clear();
    m_imgUrls.clear();

    for(const QString &img : imgs) {
        if (img.indexOf('-')>0) {
            QStringList sl = img.split('-');
            bool ok;
            int page = sl.last().toInt(&ok);
            if (ok && page>0)
                m_imgList[sl.first()].append(page);
        } else
            m_imgList[img].append(1);
    }

    m_worksPageLoad = m_imgList.count();
    m_worksImgFetch = 0;
    for(auto it  = m_imgList.constBegin(), end = m_imgList.constEnd(); it != end; ++it) {
        QUrl url(QSL("https://www.pixiv.net/member_illust.php"));
        QUrlQuery qr;
        qr.addQueryItem(QSL("mode"),QSL("manga"));
        qr.addQueryItem(QSL("illust_id"),it.key());
        url.setQuery(qr);
        QTimer::singleShot(0,gSet->auxNetworkAccessManager(),[this,url]{
            QNetworkRequest req(url);
            req.setRawHeader("referer",m_origin.toString().toUtf8());
            QNetworkReply* rpl = gSet->auxNetworkAccessManager()->get(req);
            connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
        });
    }
}

QJsonDocument CPixivNovelExtractor::parseJsonSubDocument(const QByteArray& source, const QString& start)
{
    QJsonDocument doc;

    const QByteArray baStart = start.toUtf8();
    int idx = source.indexOf(baStart);
    if (idx<0) {
        doc = QJsonDocument::fromJson(R"({"error":"Unable to find JSON sub-document."})");
        return doc;
    }
    QByteArray cnt = source.mid(idx+baStart.size()-1);

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

QStringList CPixivNovelExtractor::parseJsonIllustPage(const QString &html, const QUrl &origin)
{
    QStringList res;
    QJsonDocument doc;

    QUrlQuery qr(origin);
    QString key = qr.queryItemValue(QSL("illust_id"));
    QString jstart = QSL("illust: { %1: {").arg(key);
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

        res << QSL("%1/%2_p%3.%4")
               .arg(path,illustId,QString::number(i),suffix);
    }

    return res;
}

QStringList CPixivNovelExtractor::parseIllustPage(const QString &html, bool mediumMode)
{
    QStringList res;
    int pos = 0;

    QRegExp rx(QSL("data-src=\\\".*\\\""),Qt::CaseInsensitive);
    if (mediumMode) {
        rx = QRegExp(QSL(
                         "\\\"https:\\\\/\\\\/i\\.pximg\\.net\\\\/img-original\\\\/.*\\\""),
                     Qt::CaseInsensitive);
    }
    rx.setMinimal(true);

    while ((pos = rx.indexIn(html, pos)) != -1 )
    {
        QString u = rx.cap(0);
        if (mediumMode) {
            u.remove('\\');
        } else {
            u.remove(QSL("data-src="),Qt::CaseInsensitive);
        }
        u.remove('\"');
        res << u;
        pos += rx.matchedLength();
    }

    return res;
}

QString CPixivNovelExtractor::parseJsonNovel(const QString &html, QStringList &tags,
                                             QString &author, QString &authorNum,
                                             QString &title)
{
    QByteArray cnt = html.toUtf8();
    QString res;

    QJsonDocument doc = parseJsonSubDocument(cnt,QSL("%1: {").arg(m_novelId));
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

    doc = parseJsonSubDocument(cnt,QSL("%1: {").arg(authorNum));
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
