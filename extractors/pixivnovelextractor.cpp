#include <utility>

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
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/network.h"
#include "global/browserfuncs.h"

namespace CDefaults {
const auto propPixivWTitle = "PIXIV_WTITLE";
const auto propPixivIllustID = "PIXIV_ILLUSTID";
const auto propPixivIllustKey = "PIXIV_ILLUSTKEY";
const auto propPixivTitle = "PIXIV_TITLE";
const auto propPixivDesc = "PIXIV_DESC";
}

CPixivNovelExtractor::CPixivNovelExtractor(QObject *parent)
    : CAbstractExtractor(parent)
{
}

void CPixivNovelExtractor::setParams(const QUrl &source, const QString &title,
                                     bool translate, bool alternateTranslate, bool focus,
                                     bool downloadNovel, const CStringHash &auxInfo)
{
    m_title = title;
    m_translate = translate;
    m_alternateTranslate = alternateTranslate;
    m_focus = focus;
    m_source = source;
    m_downloadNovel = downloadNovel;
    m_auxInfo = auxInfo;
}

void CPixivNovelExtractor::setMangaParams(const QUrl &origin, bool useViewer, bool focus)
{
    m_mangaOrigin = origin;
    m_useMangaViewer = useViewer;
    m_focus = focus;
}

QString CPixivNovelExtractor::workerDescription() const
{
    return tr("Pixiv novel extractor");
}

void CPixivNovelExtractor::startMain()
{
    m_finishingLock = false;
    if (m_mangaOrigin.isValid()) {
        QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
            if (exitIfAborted()) return;
            QNetworkRequest req(m_mangaOrigin);
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
            QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);
            connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivNovelExtractor::loadError);
            connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
        },Qt::QueuedConnection);

    } else if (m_source.isValid()){
        QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
            if (exitIfAborted()) return;
            QNetworkRequest req(m_source);
            QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

            connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivNovelExtractor::loadError);
            connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::novelLoadFinished);
        },Qt::QueuedConnection);

    } else {
        Q_EMIT finished();
    }
    m_redirectCounter.clear();
}

void CPixivNovelExtractor::novelLoadFinished()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;
    if (exitIfAborted()) return;

    if (rpl->error() == QNetworkReply::NoError) {
        const QByteArray data = rpl->readAll();
        QString html = QString::fromUtf8(data);
        addLoadedRequest(data.size());

        QUrl origin = rpl->url();
        QUrlQuery qr(origin);

        QString hauthor;
        QString hauthornum;
        QString htitle;
        QString hdescription;
        QStringList tags;
        CStringHash embImages;
        QDateTime createDate;
        QString wtitle = m_title;

        if (qr.hasQueryItem(QSL("id"))) {
            m_novelId = qr.queryItemValue(QSL("id"));

            if (wtitle.isEmpty())
                wtitle = CGenericFuncs::extractFileTitle(html);

            static const QRegularExpression rxToken(QSL("\\{\\s*\\\"token\\\"\\s*:"));
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

                html = parseJsonNovel(html,tags,hauthor,hauthornum,htitle,embImages,createDate,hdescription);

            } else {
                // Try new JSON call (2025)
                QUrl url(QSL("https://www.pixiv.net/ajax/novel/%1").arg(m_novelId));
                QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,wtitle,origin]{
                    QNetworkRequest req(url);
                    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
                    req.setRawHeader("referer",m_origin.toString().toUtf8());
                    QNetworkReply* nrpl = gSet->net()->auxNetworkAccessManagerGet(req);
                    nrpl->setProperty(CDefaults::propPixivWTitle,wtitle);
                    m_originalOrigin = origin;
                    connect(nrpl,&QNetworkReply::errorOccurred,this,&CPixivNovelExtractor::loadError);
                    connect(nrpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::novelLoadFinished);
                },Qt::QueuedConnection);
                return;
            }

        } else {
            QString ttitle = rpl->property(CDefaults::propPixivWTitle).toString();
            if (!ttitle.isEmpty())
                wtitle = ttitle;
            html = parseJsonNovel(html,tags,hauthor,hauthornum,htitle,embImages,createDate,hdescription);

        }

        static const QRegularExpression rbrx(QSL("\\[\\[rb\\:.*?\\]\\]"));
        int pos = 0;
        QRegularExpressionMatch match = rbrx.match(html,pos);
        while (match.hasMatch()) {
            QString rb = match.captured();
            rb.replace(QSL("&gt;"), QSL(">"));
            static const QRegularExpression rbStart(QSL("^.*rb\\:"));
            static const QRegularExpression rbStop(QSL("\\>.*"));
            rb.remove(rbStart);
            rb.remove(rbStop);
            rb = rb.trimmed();
            if (!rb.isEmpty()) {
                html.replace(match.capturedStart(),match.capturedLength(),rb);
            } else {
                pos += match.capturedLength();
            }
            match = rbrx.match(html,pos);
        }

        static const QRegularExpression imrx(QSL("\\[pixivimage:\\S+\\]"));
        QStringList imgs;
        auto it = imrx.globalMatch(html);
        while (it.hasNext()) {
            match = it.next();
            QString im = match.captured();
            static const QRegularExpression pixivimageLabel(QSL(".*:"));
            im.remove(pixivimageLabel);
            im.remove(u']');
            im = im.trimmed();
            if (!im.isEmpty())
                imgs << im;
        }

        m_title = wtitle;
        m_origin = origin;
        handleImages(imgs,embImages,rpl->url());

        if (!tags.isEmpty()) {
            QString tagList;
            for (const auto& tag : std::as_const(tags)) {
                if (!tagList.isEmpty())
                    tagList.append(QSL(" / "));
                tagList.append(QSL("<a href=\"https://www.pixiv.net/tags/%1/novels\">%1</a>").arg(tag));
            }
            if (!tagList.isEmpty())
                html.prepend(QSL("Tags: %1\n\n").arg(tagList));
        }
        if (!hdescription.isEmpty()) {
            html.prepend(QSL("Description: %1\n\n").arg(CGenericFuncs::decodeHtmlEntities(hdescription)));
        }
        if (createDate.isValid() && !createDate.isNull()) {
            html.prepend(QSL("Created at: %1\n\n")
                         .arg(QLocale::c().toString(createDate,QLocale::ShortFormat)));
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

void CPixivNovelExtractor::subLoadFinished()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    if (rpl.isNull()) return;
    if (exitIfAborted()) return;

    QUrl rplUrl = rpl->url();
    QString key = rplUrl.fileName();

    if (rplUrl.path().endsWith(QSL("/pages")))
        key = rpl->property(CDefaults::propPixivIllustKey).toString();

    bool ok = false;
    int tkey = key.toInt(&ok);
    if (key.isEmpty() || !ok || (tkey <= 0)) {
        qCritical() << "Illust key lost!";
        return;
    }

    int httpStatus = CGenericFuncs::getHttpStatusFromReply(rpl.data());

    if ((rpl->error() == QNetworkReply::NoError) && (httpStatus<CDefaults::httpCodeRedirect)) {
        // valid page without redirect or error
        const CIntList idxs = m_imgList.value(key);
        const QByteArray data = rpl->readAll();
        QString html = QString::fromUtf8(data);
        addLoadedRequest(data.size());
        m_redirectCounter.remove(key);

        QString illustID;
        QString title;
        QString description;
        bool originalScale = true;
        QVector<CUrlWithName> imageUrls;

        if (rplUrl.path().endsWith(QSL("/pages"))) {
            illustID = rpl->property(CDefaults::propPixivIllustID).toString();
            title = rpl->property(CDefaults::propPixivTitle).toString();
            description = rpl->property(CDefaults::propPixivDesc).toString();

            imageUrls = parseJsonIllustListPage(html,&originalScale);
        } else {
            imageUrls = parseJsonIllustPage(html,rplUrl,&illustID,&title,&description,&originalScale);

            if (imageUrls.isEmpty() && !illustID.isEmpty()) {
                QUrl url(QSL("https://www.pixiv.net/ajax/illust/%1/pages").arg(illustID));
                QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,illustID,title,description,key]{
                    QNetworkRequest req(url);
                    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
                    req.setRawHeader("referer",m_origin.toString().toUtf8());
                    QNetworkReply* nrpl = gSet->net()->auxNetworkAccessManagerGet(req);
                    nrpl->setProperty(CDefaults::propPixivIllustID,illustID);
                    nrpl->setProperty(CDefaults::propPixivTitle,title);
                    nrpl->setProperty(CDefaults::propPixivDesc,description);
                    nrpl->setProperty(CDefaults::propPixivIllustKey,key);
                    connect(nrpl,&QNetworkReply::errorOccurred,this,&CPixivNovelExtractor::loadError);
                    connect(nrpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
                },Qt::QueuedConnection);
                return;
            }
        }

        // Aux manga load from context menu
        if (m_mangaOrigin.isValid()) {
            Q_EMIT mangaReady(imageUrls,illustID,m_mangaOrigin,title,description,m_useMangaViewer,m_focus,originalScale);
            Q_EMIT finished();
            return;
        }

        for (auto it = idxs.constBegin(), end = idxs.constEnd(); it != end; ++it) {
            int page = (*it);
            if (page<1 || page>imageUrls.count()) continue;

            const QUrl url = QUrl(imageUrls.at(page-1).first);
            const QString pageId = QSL("%1_%2").arg(key).arg(page);
            pixivDirectFetchImage(url,rplUrl,pageId);
        }
    } else if (httpStatus>=CDefaults::httpCodeRedirect && httpStatus<CDefaults::httpCodeClientError) {
        // redirect
        m_redirectCounter[key]++;
        if (m_redirectCounter.value(key)>CDefaults::httpMaxRedirects) {
            qCritical() << "Too many redirects for " << rplUrl;
        } else {
            QUrl url(QString::fromLatin1(rpl->rawHeader("Location")));
            if (url.isRelative()) {
                QUrl base = gSet->browser()->cleanUrlForRealm(rplUrl); // (QSL("https://www.pixiv.net"));
                url = base.resolved(url);
            }
            if (url.isValid() && !url.isRelative() && !url.isLocalFile()) {
                QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url]{
                    QNetworkRequest req(url);
                    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
                    req.setRawHeader("referer",m_origin.toString().toUtf8());
                    QNetworkReply* nrpl = gSet->net()->auxNetworkAccessManagerGet(req);
                    connect(nrpl,&QNetworkReply::errorOccurred,this,&CPixivNovelExtractor::loadError);
                    connect(nrpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
                },Qt::QueuedConnection);
                return;
            }
            qWarning() << "Failed to redirect to " << rpl->rawHeader("Location");
            qDebug() << rpl->rawHeaderPairs();
        }
    } else {
        m_redirectCounter.remove(key);
        qWarning() << "Failed to fetch image from " << rplUrl;
        qDebug() << rpl->rawHeaderPairs();
    }
    QMetaObject::invokeMethod(this,&CPixivNovelExtractor::subWorkFinished,Qt::QueuedConnection);
    m_worksPageLoad--;
}

void CPixivNovelExtractor::pixivDirectFetchImage(const QUrl& url, const QUrl& referer, const QString& pageId)
{
    static const QStringList &supportedExt = CGenericFuncs::getSupportedImageExtensions();
    QMutexLocker locker(&m_imgMutex);

    m_imgUrls[pageId] = url.toString();

    QFileInfo fi(url.toString());
    if (gSet->settings()->pixivFetchImages &&
            supportedExt.contains(fi.suffix(),Qt::CaseInsensitive)) {
        m_worksImgFetch++;
        QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,referer]{
            if (exitIfAborted()) return;
            QNetworkRequest req(url);
            req.setRawHeader("referer",referer.toString().toUtf8());
            QNetworkReply *rplImg = gSet->net()->auxNetworkAccessManagerGet(req);
            connect(rplImg,&QNetworkReply::errorOccurred,this,&CPixivNovelExtractor::loadError);
            connect(rplImg,&QNetworkReply::finished,this,&CPixivNovelExtractor::subImageFinished);
        },Qt::QueuedConnection);
    }
}

void CPixivNovelExtractor::subWorkFinished()
{
    if (m_worksPageLoad>0 || m_worksImgFetch>0) return;
    if (m_finishingLock) return;

    m_finishingLock = true;

    // replace fetched image urls
    for (auto it = m_imgUrls.constBegin(), end = m_imgUrls.constEnd(); it != end; ++it) {
        QRegularExpression rx;
        if (it.key().startsWith(QSL("E#"))) {
            rx = QRegularExpression(QSL("\\[uploadedimage:%1\\]").arg(it.key().mid(2)));
        } else {
            const QStringList kl = it.key().split(u'_');
            if (kl.last() == QSL("1")) {
                rx = QRegularExpression(QSL("\\[pixivimage:%1(?:-1)?\\]").arg(kl.first()));
            } else {
                rx = QRegularExpression(QSL("\\[pixivimage:%1-%2\\]")
                                        .arg(kl.first(),kl.last()));
            }
        }

        QString imgHtml = QSL("<br /><img src=\"%1\"").arg(it.value());
        if (it.value().startsWith(QSL("data:"))) {
            imgHtml.append(QSL("/ ><br />"));
        } else {
            imgHtml.append(QSL(" width=\"%1px;\"/ ><br />")
                       .arg(gSet->settings()->pdfImageMaxSize));
        }
        m_html.replace(rx, imgHtml);
    }

    CStringHash info = m_auxInfo;
    info.insert(QSL("title"), m_title);
    info.insert(QSL("id"), QSL("%1").arg(m_novelId));
    QUrl tmpOrigin = m_origin;
    if (!m_originalOrigin.isEmpty() && m_originalOrigin.isValid())
        tmpOrigin = m_originalOrigin;
    Q_EMIT novelReady(CGenericFuncs::makeSimpleHtml(m_title,m_html,true,tmpOrigin),m_focus,
                      m_translate,m_alternateTranslate,m_downloadNovel,info);
    Q_EMIT finished();
}

void CPixivNovelExtractor::subImageFinished()
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

void CPixivNovelExtractor::handleImages(const QStringList &imgs, const CStringHash &embImgs, const QUrl &mainReferer)
{
    m_imgList.clear();
    m_imgUrls.clear();
    m_worksImgFetch = 0;

    for(auto it = embImgs.constKeyValueBegin(), end = embImgs.constKeyValueEnd(); it != end; ++it)
        pixivDirectFetchImage(QUrl(it->second),mainReferer,QSL("E#%1").arg(it->first));

    for(const QString &img : imgs) {
        if (img.indexOf(u'-')>0) {
            QStringList sl = img.split(u'-');
            bool ok = false;
            int page = sl.last().toInt(&ok);
            if (ok && page>0)
                m_imgList[sl.first()].append(page);
        } else {
            m_imgList[img].append(1);
        }
    }

    m_worksPageLoad = m_imgList.count();
    for(auto it  = m_imgList.constBegin(), end = m_imgList.constEnd(); it != end; ++it) {
        QUrl url(QSL("https://www.pixiv.net/artworks/%1").arg(it.key()));
        QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url]{
            if (exitIfAborted()) return;
            QNetworkRequest req(url);
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
            req.setRawHeader("referer",m_origin.toString().toUtf8());
            QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);
            connect(rpl,&QNetworkReply::errorOccurred,this,&CPixivNovelExtractor::loadError);
            connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
        },Qt::QueuedConnection);
    }
}

QVector<CUrlWithName> CPixivNovelExtractor::parseJsonIllustListPage(const QString &html, bool *mangaOriginalScale)
{
    QVector<CUrlWithName> res;

    QJsonDocument doc = parseJsonSubDocument(html.toUtf8(),QRegularExpression());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();

        QString err = obj.value(QSL("error")).toString();
        if (!err.isNull()) {
            qWarning() << QSL("AJAX images extractor error: %1").arg(err);
            return res;
        }

        QString pageUrlSelector = QSL("original");
        if (mangaOriginalScale != nullptr)
            (*mangaOriginalScale) = true;
        if (m_useMangaViewer) {
            switch (gSet->settings()->pixivMangaPageSize) {
            case CStructures::PixivMangaPageSize::pxmpOriginal:
                pageUrlSelector = QSL("original");
                if (mangaOriginalScale != nullptr)
                    (*mangaOriginalScale) = true;
                break;
            case CStructures::PixivMangaPageSize::pxmpRegular:
                pageUrlSelector = QSL("regular");
                if (mangaOriginalScale != nullptr)
                    (*mangaOriginalScale) = false;
                break;
            case CStructures::PixivMangaPageSize::pxmpSmall:
                pageUrlSelector = QSL("small");
                if (mangaOriginalScale != nullptr)
                    (*mangaOriginalScale) = false;
                break;
            }
        }

        const QJsonArray urls = obj.value(QSL("body")).toArray();
        for (const auto &item : urls) {
            const QString imgUrl = item.toObject().value(QSL("urls")).toObject().value(pageUrlSelector).toString();
            res.append(qMakePair(imgUrl, QString()));
        }
    }

    return res;
}

QVector<CUrlWithName> CPixivNovelExtractor::parseJsonIllustPage(const QString &html, const QUrl &origin,
                                                                QString* id, QString* title,
                                                                QString* description, bool* mangaOriginalScale)
{
    QVector<CUrlWithName> res;
    QJsonDocument doc;

    QString key = origin.fileName();
    if (id != nullptr)
        *id = key;

    QRegularExpression jstart;
    jstart = QRegularExpression(QSL("\\s*\\\"illust\\\"\\s*:\\s*{\\s*\\\"%1\\\"\\s*:\\s*{").arg(key)); // NOLINT
    int idx = html.indexOf(jstart);
    bool scheme1 = true;
    if (idx < 0) {
        // Try new scheme
        scheme1 = false;
        jstart = QRegularExpression(QSL("\\\"props\\\"\\s*:\\s*{")); // NOLINT
        idx = html.indexOf(jstart);
    }
    if (idx < 0) {
        return res;
    }

    QString illustId;
    QString suffix;
    QString path;
    QString szSelector;
    int pageCount = -1;

    doc = parseJsonSubDocument(html.toUtf8(),jstart);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();

        QString err = obj.value(QSL("error")).toString();
        if (!err.isNull()) {
            qWarning() << QSL("Images extractor error: %1").arg(err);
            return res;
        }

        if (scheme1) {
            illustId = obj.value(QSL("illustId")).toString();

            if (title != nullptr)
                *title = obj.value(QSL("title")).toString();

            if (description != nullptr) {
                const QDateTime dt = QDateTime::fromString(obj.value(QSL("createDate")).toString(),
                                                           Qt::ISODate);
                description->clear();

                const QJsonArray jtags = obj.value(QSL("tags")).toObject().value(QSL("tags")).toArray();
                QStringList tags;
                tags.reserve(jtags.count());
                for (const auto& tag : jtags)
                    tags.append(tag.toObject().value(QSL("tag")).toString());
                if (!tags.isEmpty())
                    description->append(QSL("<b>Tags:</b> %1.<br/>").arg(tags.join(QSL(" / "))));

                description->append(tr("<b>Author:</b> <a href=\"https://www.pixiv.net/users/%1\">%2</a><br/>")
                                        .arg(obj.value(QSL("userId")).toString(),
                                             obj.value(QSL("userName")).toString()));

                description->append(tr("<b>Size:</b> %1x%2 px, <b>created at:</b> %3.<br/>")
                                        .arg(obj.value(QSL("width")).toInt())
                                        .arg(obj.value(QSL("height")).toInt())
                                        .arg(dt.toString(QSL("yyyy/MM/dd hh:mm"))));
            }

        } else {
            illustId = key;

            if (title != nullptr) {
                QRegularExpression rxTitle(QSL("\\<title.*\\>(?<title>.*?)\\<\\/title\\>"));
                QRegularExpressionMatch match = rxTitle.match(html);
                if (match.hasMatch()) {
                    *title = match.captured(QSL("title"));
                }
            }

            if (description != nullptr)
                description->clear();
        }

        pageCount = qRound(obj.value(QSL("pageCount")).toDouble(-1.0));

        QString pageUrlSelector = QSL("original");
        if (mangaOriginalScale != nullptr)
            (*mangaOriginalScale) = true;
        if (m_useMangaViewer) {
            switch (gSet->settings()->pixivMangaPageSize) {
                case CStructures::PixivMangaPageSize::pxmpOriginal:
                    pageUrlSelector = QSL("original");
                    szSelector.clear();
                    if (mangaOriginalScale != nullptr)
                        (*mangaOriginalScale) = true;
                    break;
                case CStructures::PixivMangaPageSize::pxmpRegular:
                    pageUrlSelector = QSL("regular");
                    szSelector = QSL("_master1200");
                    if (mangaOriginalScale != nullptr)
                        (*mangaOriginalScale) = false;
                    break;
                case CStructures::PixivMangaPageSize::pxmpSmall:
                    pageUrlSelector = QSL("small");
                    szSelector = QSL("_master1200");
                    if (mangaOriginalScale != nullptr)
                        (*mangaOriginalScale) = false;
                    break;
            }
        }
        const QString fname = obj.value(QSL("urls")).toObject()
                              .value(pageUrlSelector).toString();
        if (!fname.isNull()) {
            QFileInfo fi(fname);
            path = fi.path();
            suffix = fi.completeSuffix();
        }
    }

    for (int i=0;i<pageCount;i++) {
        if (path.isEmpty() || illustId.isEmpty() || suffix.isEmpty())
            continue;

        res.append(qMakePair(QSL("%1/%2_p%3%4.%5")
                             .arg(path,illustId,QString::number(i),szSelector,suffix),
                             QString()));
    }

    return res;
}

QString CPixivNovelExtractor::parseJsonNovel(const QString &html, QStringList &tags,
                                             QString &author, QString &authorNum,
                                             QString &title, CStringHash& embeddedImages,
                                             QDateTime &createDate, QString &description)
{
    QByteArray cnt = html.toUtf8();
    QString res;

    bool novelJSONcall = html.startsWith(QChar('{'));

    QRegularExpression rxNovel(QSL("\\s*\\\"%1\\\"\\s*:\\s*{").arg(m_novelId));
    QJsonDocument doc;
    if (novelJSONcall) {
        doc = parseJsonSubDocument(cnt,QRegularExpression());
    } else {
        doc = parseJsonSubDocument(cnt,rxNovel);
    }

    if (doc.isObject()) {
        QJsonObject obj = doc.object();

        QString err = obj.value(QSL("error")).toString();
        if (!err.isNull())
            return QSL("Novel extractor error: %1").arg(err);

        if (novelJSONcall) {
            obj = obj.value(QSL("body")).toObject();
            author = obj.value(QSL("userName")).toString();
        }

        res = obj.value(QSL("content")).toString();
        title = obj.value(QSL("title")).toString();
        authorNum = obj.value(QSL("userId")).toString();
        createDate = QDateTime::fromString(obj.value(QSL("createDate")).toString(),Qt::ISODate);
        description = obj.value(QSL("description")).toString();

        const QJsonArray vtags = obj.value(QSL("tags")).toObject()
                                 .value(QSL("tags")).toArray();
        for(const auto &tag : vtags) {
            QString t = tag.toObject().value(QSL("tag")).toString();
            if (!t.isEmpty())
                tags.append(t);
        }

        const QJsonObject embImages = obj.value(QSL("textEmbeddedImages")).toObject();
        const QStringList embImagesList = embImages.keys();
        for(int i=0; i<embImagesList.count(); i++) {
            const QString &id = embImagesList.at(i);
            const QString url = embImages.value(id).toObject()
                                .value(QSL("urls")).toObject()
                                .value(QSL("original")).toString();
            embeddedImages.insert(id,url);
        }
    } else {
        return tr("ERROR: JSON - Unable to find novel subdocument.");

    }

    if (author.isEmpty() && !novelJSONcall) {
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
    }

    return res;
}
