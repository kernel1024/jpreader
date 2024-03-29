#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <algorithm>

#include "patreonextractor.h"
#include "utils/genericfuncs.h"

CPatreonExtractor::CPatreonExtractor(QObject *parent)
    : CAbstractExtractor(parent)
{
}

void CPatreonExtractor::setParams(const QString &pageHtml, const QUrl &origin, bool extractAttachments)
{
    m_html = pageHtml;
    m_origin = origin;
    m_extractAttachments = extractAttachments;
}

QString CPatreonExtractor::workerDescription() const
{
    return tr("Patreon extractor");
}

void CPatreonExtractor::startMain()
{
    if (m_html.isEmpty()) {
        showError(tr("HTML document is empty."));
        return;
    }

    QString html = m_html;
    const QString start = QSL("<script id=\"__NEXT_DATA__\" type=\"application/json\">");
    int pos = html.indexOf(start);
    if (pos<0) {
        showError(tr("Patreon post bootstrap not found."));
        return;
    }
    html.remove(0,pos+start.length());

    pos = html.indexOf(QSL("</script>"));
    if (pos<0) {
        showError(tr("Patreon post bootstrap end sequence not found."));
        return;
    }
    html.truncate(pos);
    html = html.trimmed();

    QJsonParseError err {};
    QJsonDocument doc = QJsonDocument::fromJson(html.toUtf8(),&err);
    if (doc.isNull()) {
        showError(tr("JSON parser error %1 at %2: %3.")
                  .arg(err.error)
                  .arg(err.offset)
                  .arg(err.errorString()));
        return;
    }

    using COrderedUrlWithName = QPair<int,CUrlWithName>;
    QVector<COrderedUrlWithName> urls;
    QString title = m_origin.fileName();
    if (doc.isObject()) {
        const QJsonObject bootstrap = doc.object()
                                          .value(QSL("props"))
                                          .toObject()
                                          .value(QSL("pageProps"))
                                          .toObject()
                                          .value(QSL("bootstrapEnvelope"))
                                          .toObject()
                                          .value(QSL("bootstrap"))
                                          .toObject();

        const QJsonObject postAttributes = bootstrap.value(QSL("post")).toObject()
                                           .value(QSL("data")).toObject()
                                           .value(QSL("attributes")).toObject();
        QString s = postAttributes.value(QSL("title")).toString();
        if (!s.isEmpty())
            title = s;

        QHash<QString,int> sortOrder;
        if (!m_extractAttachments) {
            const QJsonArray imageOrder = postAttributes.value(QSL("post_metadata")).toObject()
                         .value(QSL("image_order")).toArray();
            for (int i=0;i<imageOrder.count();i++) {
                const QString key = imageOrder.at(i).toString();
                if (!key.isEmpty())
                    sortOrder[key] = i;
            }
        }

        const QJsonArray jincluded = bootstrap.value(QSL("post")).toObject().value(QSL("included")).toArray();
        int idx = 0;
        urls.reserve(jincluded.count());
        for (const auto &jinc : jincluded) {
            const QString type = jinc.toObject().value(QSL("type")).toString().toLower();

            if (type == QSL("media") && (!m_extractAttachments)) {
                const QJsonObject attrib = jinc.toObject().value(QSL("attributes")).toObject();

                const QString url = attrib.value(QSL("download_url")).toString();
                QString filename = attrib.value(QSL("file_name")).toString();
                const QString id = jinc.toObject().value(QSL("id")).toString();

                if (!url.isEmpty()) {
                    if (filename.isEmpty()) {
                        filename = CGenericFuncs::paddedNumber(idx,jincluded.count());
                        const QFileInfo fi(QUrl(url).path());
                        const QString suffix = fi.suffix();
                        if (!suffix.isEmpty())
                            filename.append(QSL(".%1").arg(suffix));
                    }

                    urls.append(qMakePair(sortOrder.value(id,-1),
                                          qMakePair(url,filename)));
                }
                idx++;

            } else if (type == QSL("attachment") && m_extractAttachments) {
                const QString url = jinc.toObject().value(QSL("attributes")).toObject().value(QSL("url")).toString();
                QString filename = jinc.toObject().value(QSL("attributes")).toObject().value(QSL("name")).toString();

                if (!url.isEmpty()) {
                    if (filename.isEmpty())
                        filename = CGenericFuncs::paddedNumber(idx,jincluded.count());

                    urls.append(qMakePair(idx,qMakePair(url,filename)));
                }
                idx++;
            }
        }
    }

    if (!urls.isEmpty()) {
        std::sort(urls.begin(),urls.end(),
                  [](const COrderedUrlWithName& u1, const COrderedUrlWithName& u2){
            if (u1.first>=0 && u2.first>=0) // Compare with predefined order
                return (u1.first < u2.first);
            if (!u1.second.second.isEmpty() && !u2.second.second.isEmpty()) // Compare by filenames
                return (u1.second.second < u2.second.second);
            // Fallback compare by url as strings
            return (u1.second.first < u2.second.first);
        });

        QVector<CUrlWithName> res;
        res.reserve(urls.count());
        for (const auto& url : qAsConst(urls))
            res.append(url.second);

        Q_EMIT mangaReady(res,title,m_origin,title,QString(),false,false,true);
    }

    Q_EMIT finished();
}
