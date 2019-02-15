#include <QApplication>
#include <QString>
#include <QMessageBox>
#include <QUrlQuery>
#include <QFileInfo>
#include <QBuffer>

#include "pixivnovelextractor.h"
#include "genericfuncs.h"
#include "globalcontrol.h"

CPixivNovelExtractor::CPixivNovelExtractor(QObject *parent)
    : QObject(parent), m_translate(false), m_focus(false), m_snv(nullptr)
{

}

void CPixivNovelExtractor::setParams(CSnippetViewer *viewer, const QString &title,
                                     bool translate, bool focus)
{
    m_snv = viewer;
    m_title = title;
    m_translate = translate;
    m_focus = focus;
}

void CPixivNovelExtractor::novelLoadFinished()
{
    QNetworkReply *rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==nullptr || m_snv==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QString html = QString::fromUtf8(rpl->readAll());

        QString wtitle = m_title;
        if (wtitle.isEmpty())
            wtitle = extractFileTitle(html);

        QString htitle;
        QRegExp hrx("<h1[^>]*class=\"title\"[^>]*>",Qt::CaseInsensitive);
        int hidx = hrx.indexIn(html);
        if (hidx>=0) {
            int hpos = hidx+hrx.matchedLength();
            int hlen = html.indexOf('<',hpos) - hpos;
            htitle = html.mid(hpos, hlen);
        }

        QString hauthor;
        QString hauthornum;
        QRegExp arx("<a[^>]*class=\"user-name\"[^>]*>",Qt::CaseInsensitive);
        int aidx = arx.indexIn(html);
        if (aidx>=0) {
            QString acap = arx.cap();
            int anpos = acap.indexOf("id=")+3;
            int anlen = acap.indexOf("\"",anpos) - anpos;
            hauthornum = acap.mid(anpos,anlen);

            int apos = aidx+arx.matchedLength();
            int alen = html.indexOf('<',apos) - apos;
            hauthor = html.mid(apos, alen);
        }

        QStringList tags;
        QRegExp trx("<li[^>]*class=\"tag\"[^>]*>",Qt::CaseInsensitive);
        QRegExp ttrx("class=\"text\"[^>]*>",Qt::CaseInsensitive);
        int tpos = 0;
        int tstop = html.indexOf("template-work-tags",Qt::CaseInsensitive);
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

        QRegExp rx("<textarea[^>]*id=\"novel_text\"[^>]*>",Qt::CaseInsensitive);
        int idx = rx.indexIn(html);
        if (idx<0) return;
        html.remove(0,idx+rx.matchedLength());
        idx = html.indexOf(QString("</textarea>"),0,Qt::CaseInsensitive);
        if (idx>=0)
            html.truncate(idx);

        QRegExp rbrx("\\[\\[rb\\:.*\\]\\]");
        rbrx.setMinimal(true);
        int pos = 0;
        while ((pos = rbrx.indexIn(html,pos)) != -1) {
            QString rb = rbrx.cap(0);
            rb.replace("&gt;", ">");
            rb.remove(QRegExp("^.*rb\\:"));
            rb.remove(QRegExp("\\>.*"));
            rb = rb.trimmed();
            if (!rb.isEmpty())
                html.replace(pos,rbrx.matchedLength(),rb);
            else
                pos += rbrx.matchedLength();
        }

        QRegExp imrx("\\[pixivimage\\:\\S+\\]");
        pos = 0;
        QStringList imgs;
        while ((pos = imrx.indexIn(html,pos)) != -1) {
            QString im = imrx.cap(0);
            im.remove(QRegExp(".*\\:"));
            im.remove(']');
            im = im.trimmed();
            if (!im.isEmpty())
                imgs << im;
            pos += imrx.matchedLength();
        }

        m_title = wtitle;
        m_origin = rpl->url();
        handleImages(imgs);

        if (!tags.isEmpty())
            html.prepend(QString("Tags: %1\n\n").arg(tags.join(" / ")));
        if (!hauthor.isEmpty())
            html.prepend(QString("Author: <a href=\"https://www.pixiv.net/member.php?id=%1\">%2</a>\n\n")
                         .arg(hauthornum,hauthor));
        if (!htitle.isEmpty())
            html.prepend(QString("Title: <b>%1</b>\n\n").arg(htitle));

        m_html = html;

        subWorkFinished(); // call just in case for empty lists
    }
    rpl->deleteLater();
}

void CPixivNovelExtractor::novelLoadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)
    qApp->restoreOverrideCursor();

    QString msg("Unable to load pixiv novel.");

    QNetworkReply* rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl!=nullptr)
        msg.append(QString(" %1").arg(rpl->errorString()));

    QWidget *w = nullptr;
    if (m_snv!=nullptr)
        w = m_snv->parentWnd;
    QMessageBox::warning(w,tr("JPReader"),msg);
    deleteLater();
}

void CPixivNovelExtractor::subLoadFinished()
{
    QNetworkReply *rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==nullptr) return;

    QUrlQuery qr(rpl->url());
    QString key = qr.queryItemValue("illust_id");
    QString mode = qr.queryItemValue("mode");
    bool mediumMode = (QString("medium").compare(mode,Qt::CaseInsensitive) == 0);

    if (rpl->error() == QNetworkReply::NoError) {

        QString html = QString::fromUtf8(rpl->readAll());
        const QSet<int> idxs = m_imgList.value(key);
        int pos = 0;

        QRegExp rx("data-src=\\\".*\\\"",Qt::CaseInsensitive);
        if (mediumMode)
            rx = QRegExp("\\\"https:\\\\/\\\\/i\\.pximg\\.net\\\\/img-original\\\\/.*\\\"",Qt::CaseInsensitive);
        rx.setMinimal(true);

        QStringList imageUrls;
        while ((pos = rx.indexIn(html, pos)) != -1 )
        {
            QString u = rx.cap(0);
            if (mediumMode)
                u.remove("\\");
            else
                u.remove("data-src=",Qt::CaseInsensitive);
            u.remove("\"");
            imageUrls << u;
            pos += rx.matchedLength();
        }

        m_imgMutex.lock();
        for (int i=0;i<idxs.values().count();i++) {
            int page = idxs.values().at(i);
            if (page<1 || page>imageUrls.count()) continue;
            QUrl url = QUrl(imageUrls.at(page-1));
            m_imgUrls[QString("%1_%2").arg(key).arg(page)] = url.toString();

            QFileInfo fi(url.toString());
            QStringList supportedExt;
            supportedExt << "jpeg" << "jpg" << "gif" << "bmp" << "png";
            if (gSet->settings.pixivFetchImages &&
                    supportedExt.contains(fi.suffix(),Qt::CaseInsensitive)) {
                m_worksImgFetch++;
                QNetworkRequest req(url);
                req.setRawHeader("referer",rpl->url().toString().toUtf8());
                QNetworkReply *rplImg = gSet->auxNetManager->get(req);
                connect(rplImg,&QNetworkReply::finished,this,&CPixivNovelExtractor::subImageFinished);
            }
        }
        m_imgMutex.unlock();
    } else {
        QVariant vstatus = rpl->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (vstatus.isValid()) {
            bool ok;
            int status = vstatus.toInt(&ok);
            if (ok && status>=400 && status<500) {
                // not found... try single page load
                QUrl url(QString("https://www.pixiv.net/member_illust.php"));
                QUrlQuery qr;
                qr.addQueryItem("mode","medium");
                qr.addQueryItem("illust_id",QString("%1").arg(key));
                url.setQuery(qr);
                QNetworkRequest req(url);
                req.setRawHeader("referer",m_origin.toString().toUtf8());
                QNetworkReply* nrpl = gSet->auxNetManager->get(req);

                connect(nrpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);

                rpl->deleteLater();
                return;
            }
        }
    }
    QMetaObject::invokeMethod(this,"subWorkFinished",Qt::QueuedConnection);
    m_worksPageLoad--;
    rpl->deleteLater();
}

void CPixivNovelExtractor::subWorkFinished()
{
    if (m_worksPageLoad>0 || m_worksImgFetch>0) return;

    qApp->restoreOverrideCursor();

    // replace fetched image urls
    for(const QString& key : m_imgUrls.keys()) {
        QStringList kl = key.split("_");

        QRegExp rx;
        if (kl.last()=="1")
            rx = QRegExp(QString("\\[pixivimage\\:%1(-1)?\\]").arg(kl.first()));
        else
            rx = QRegExp(QString("\\[pixivimage\\:%1-%2\\]").arg(kl.first(),kl.last()));

        QString rpl = QString("<img src=\"%1\"").arg(m_imgUrls.value(key));
        if (m_imgUrls.value(key).startsWith("data:"))
            rpl.append("/ >");
        else
            rpl.append(QString(" width=\"%1px;\"/ >").arg(gSet->settings.pdfImageMaxSize));
        m_html.replace(rx, rpl);
    }

    emit novelReady(makeSimpleHtml(m_title,m_html,true,m_origin),m_focus,m_translate);
    deleteLater();
}

void CPixivNovelExtractor::subImageFinished()
{
    QNetworkReply *rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QByteArray ba = rpl->readAll();

        m_imgMutex.lock();
        if (m_imgUrls.values().contains(rpl->url().toString())) {
            QImage img;

            if (img.loadFromData(ba)) {
                if (img.width()>img.height()) {
                    if (img.width()>gSet->settings.pdfImageMaxSize)
                        img = img.scaledToWidth(gSet->settings.pdfImageMaxSize,
                                                Qt::SmoothTransformation);
                } else {
                    if (img.height()>gSet->settings.pdfImageMaxSize)
                        img = img.scaledToHeight(gSet->settings.pdfImageMaxSize,
                                                 Qt::SmoothTransformation);
                }
                ba.clear();
                QBuffer buf(&ba);
                buf.open(QIODevice::WriteOnly);
                img.save(&buf,"JPEG",gSet->settings.pdfImageQuality);

                QByteArray out("data:image/jpeg;base64,");
                out.append(QString::fromLatin1(ba.toBase64()));

                for (const QString &key : m_imgUrls.keys()) {
                    if (m_imgUrls.value(key)==rpl->url().toString()) {
                        m_imgUrls[key] = QString::fromUtf8(out);
                    }
                }
            }
        }
        m_imgMutex.unlock();
    }
    QMetaObject::invokeMethod(this,"subWorkFinished",Qt::QueuedConnection);
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
                m_imgList[sl.first()].insert(page);
        } else
            m_imgList[img].insert(1);
    }

    m_worksPageLoad = m_imgList.keys().count();
    m_worksImgFetch = 0;
    for(const QString &key : m_imgList.keys()) {
        QUrl url(QString("https://www.pixiv.net/member_illust.php"));
        QUrlQuery qr;
        qr.addQueryItem("mode","manga");
        qr.addQueryItem("illust_id",QString("%1").arg(key));
        url.setQuery(qr);
        QNetworkRequest req(url);
        req.setRawHeader("referer",m_origin.toString().toUtf8());
        QNetworkReply* rpl = gSet->auxNetManager->get(req);

        connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
    }
}
