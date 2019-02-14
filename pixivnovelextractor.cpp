#include <QApplication>
#include <QString>
#include <QMessageBox>

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
    qApp->restoreOverrideCursor();

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
        QRegExp arx("<a[^>]*class=\"user-name\"[^>]*>");
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

/*        QRegExp imrx("\\[pixivimage\\:\\S+\\]");
        pos = 0;
        QStringList imgs;
        while ((pos = imrx.indexIn(html,pos)) != -1) {
            QString im = imrx.cap(0);
            im.remove(QRegExp(".*\\:"));
            im.remove(']');
            im = im.trimmed();
            if (!im.isEmpty())
                imgs << im;
            pos += rbrx.matchedLength();
        }
        handleImages(imgs,rpl->url());*/

        if (!tags.isEmpty())
            html.prepend(QString("Tags: %1\n\n").arg(tags.join(" / ")));
        if (!hauthor.isEmpty())
            html.prepend(QString("Author: <a href=\"https://www.pixiv.net/member.php?id=%1\">%2</a>\n\n")
                         .arg(hauthornum,hauthor));
        if (!htitle.isEmpty())
            html.prepend(QString("Title: <b>%1</b>\n\n").arg(htitle));

        //syncSubWorks(html);
        emit novelReady(makeSimpleHtml(wtitle,html,true,rpl->url()),m_focus,m_translate);
    }
    rpl->deleteLater();
    deleteLater();
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

void CPixivNovelExtractor::subLoadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);

    m_works--;
}

void CPixivNovelExtractor::subLoadFinished()
{
    QNetworkReply *rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==nullptr) return;

    if (rpl->error() == QNetworkReply::NoError) {
        QString key = rpl->url().toString();
        key.remove(QRegExp(".*illust_id="));

        QString html = QString::fromUtf8(rpl->readAll());
        const QSet<int> idxs = m_imgList.value(key);
        int pos = 0;
        QRegExp rx("data-src=\\\".*\\\"");
        QStringList imageUrls;
        while ((pos = rx.indexIn(html, pos)) != -1 )
        {
            imageUrls << rx.cap(0);
        }

        m_imgMutex.lock();
        for (int i=0;i<idxs.values().count();i++) {
            int page = idxs.values().at(i);
            if (page<1 || page>imageUrls.count()) continue;
            if (page==1)
                m_imgUrls[QString("[pixivimage:%1]").arg(key)] =
                        imageUrls.at(page-1);
            else
                m_imgUrls[QString("[pixivimage:%1-%2]").arg(key).arg(page)] =
                        imageUrls.at(page-1);
        }
        m_imgMutex.unlock();
        rpl->deleteLater();
    }

    m_works--;
}

void CPixivNovelExtractor::handleImages(const QStringList &imgs, const QUrl& referer)
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

    m_works = m_imgList.keys().count();
    for(const QString &key : m_imgList.keys()) {
        QUrl url(QString("https://www.pixiv.net/member_illust.php?mode=manga&illust_id=%1").arg(key));
        QNetworkRequest req(url);
        req.setRawHeader("referer",referer.toString().toUtf8());
        QNetworkReply* rpl = gSet->auxNetManager->get(req);

        connect(rpl,qOverload<QNetworkReply::NetworkError>(&QNetworkReply::error),
                this,&CPixivNovelExtractor::subLoadError);
        connect(rpl,&QNetworkReply::finished,this,&CPixivNovelExtractor::subLoadFinished);
    }
}

void CPixivNovelExtractor::syncSubWorks(QString &html)
{
    while (m_works>0)
        QThread::msleep(250);

    // replace fetched image urls
    for(const QString& key : m_imgUrls) {
        html.replace(key, QString("<img src=\"%1\"").arg(m_imgUrls.value(key)));
    }
}
