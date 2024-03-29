#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include "deviantartextractor.h"
#include "global/control.h"
#include "global/network.h"

namespace CDefaults {
const int deviantGalleryFetchCount = 24;
}

CDeviantartExtractor::CDeviantartExtractor(QObject *parent)
    : CAbstractExtractor(parent)
{
}

void CDeviantartExtractor::setParams(const QString &userID, const QString &folderID, const QString &folderName)
{
    m_userID = userID;
    m_folderID = folderID;
    m_folderName = folderName;
}

QString CDeviantartExtractor::workerDescription() const
{
    return tr("Deviantart extractor (%1/%2)").arg(m_userID,m_folderID);
}

void CDeviantartExtractor::startMain()
{
    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this]{
        m_list.clear();
        if (exitIfAborted()) return;

        QString folder = QSL("&all_folder=true&mode=newest");
        if (!m_folderID.isEmpty())
            folder = QSL("&folderid=%1").arg(m_folderID);

        QUrl u(QSL("https://www.deviantart.com/_napi/da-user-profile/api/gallery/contents"
                   "?username=%1&offset=0&limit=%2%3")
               .arg(m_userID)
               .arg(CDefaults::deviantGalleryFetchCount)
               .arg(folder));

        QNetworkRequest req(u);
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

        connect(rpl,&QNetworkReply::errorOccurred,this,&CDeviantartExtractor::loadError);
        connect(rpl,&QNetworkReply::finished,this,&CDeviantartExtractor::galleryAjax);
    },Qt::QueuedConnection);
}

void CDeviantartExtractor::galleryAjax()
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

            QUrlQuery uq(rpl->url());
            bool okconv = false;
            int offset = uq.queryItemValue(QSL("offset")).toInt(&okconv);
            if (!okconv)
                offset = -1;

            const QJsonArray tworks = obj.value(QSL("results")).toArray();

            m_list.reserve(tworks.count());
            for (const auto& work : qAsConst(tworks))
                m_list.append(work.toObject().value(QSL("deviation")).toObject());

            bool hasMore = obj.value(QSL("hasMore")).toBool();

            if (hasMore && (offset>=0) && (tworks.count()>0)) {
                // We still have unfetched links, and last fetch was not empty
                int nextOffset = offset + tworks.count();
                QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,nextOffset]{
                    if (exitIfAborted()) return;

                    QString folder = QSL("&all_folder=true&mode=newest");
                    if (!m_folderID.isEmpty())
                        folder = QSL("&folderid=%1").arg(m_folderID);

                    QUrl u(QSL("https://www.deviantart.com/_napi/da-user-profile/api/gallery/contents"
                               "?username=%1&offset=%2&limit=%3%4")
                           .arg(m_userID)
                           .arg(nextOffset)
                           .arg(CDefaults::deviantGalleryFetchCount)
                           .arg(folder));

                    QNetworkRequest req(u);
                    QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);

                    connect(rpl,&QNetworkReply::errorOccurred,this,&CDeviantartExtractor::loadError);
                    connect(rpl,&QNetworkReply::finished,this,&CDeviantartExtractor::galleryAjax);
                },Qt::QueuedConnection);
                return;
            }

            finalizeGallery();
            return;
        }
    }
    Q_EMIT finished();
}

void CDeviantartExtractor::finalizeGallery()
{
    QVector<CUrlWithName> imageUrls;
    for (const auto &jimg : qAsConst(m_list)) {
        QString url = jimg.value(QSL("media")).toObject().value(QSL("baseUri")).toString();
        if (url.isEmpty()) continue;

        QFileInfo fi(url);

        QString token = jimg.value(QSL("media")).toObject().value(QSL("token")).toArray().first().toString();
        if (token.isEmpty()) continue;

        QString prettyName = jimg.value(QSL("media")).toObject().value(QSL("prettyName")).toString();

        const QJsonArray jtypes = jimg.value(QSL("media")).toObject().value(QSL("types")).toArray();
        for (const auto &jtype : jtypes) {
            if (jtype.toObject().value(QSL("t")).toString() == QSL("fullview")) {
                QString fvurl = jtype.toObject().value(QSL("c")).toString();
                if (!fvurl.isEmpty()) {
                    fvurl.replace(QSL("<prettyName>"),prettyName);
                    if (!fvurl.startsWith(QSL("/")))
                        url.append(QSL("/"));
                    url.append(fvurl);
                    break;
                }
            }
        }

        url.append(QSL("?token=%1").arg(token));

        QString base = prettyName;
        if (base.isEmpty()) {
            QFileInfo bfi(jimg.value(QSL("url")).toString());
            base = bfi.baseName();
        }
        QString fileName = QSL("%1.%2").arg(base,fi.suffix());

        imageUrls.append(qMakePair(url,fileName));
    }

    QUrl origin(QSL("https://www.deviantart.com/%1/gallery").arg(m_userID));
    QString folder = m_userID;
    if (!m_folderName.isEmpty())
        folder.append(QSL(" - %1").arg(m_folderName));

    Q_EMIT mangaReady(imageUrls,folder,origin,m_folderName,QString(),false,false,true);
    Q_EMIT finished();
}
