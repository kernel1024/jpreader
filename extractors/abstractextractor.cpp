#include <QMessageBox>
#include "abstractextractor.h"
#include "../mainwindow.h"

#include "fanboxextractor.h"
#include "patreonextractor.h"
#include "pixivindexextractor.h"
#include "pixivnovelextractor.h"

CAbstractExtractor::CAbstractExtractor(QObject *parent, CSnippetViewer* snv)
    : QObject(parent)
{
    m_snv = snv;
}

CSnippetViewer *CAbstractExtractor::snv() const
{
    return m_snv;
}

QList<QAction *> CAbstractExtractor::addMenuActions(const QUrl &pageUrl, const QUrl &origin,
                                                    const QString &title, QMenu *menu, CSnippetViewer *snv)
{
    QList<QAction *> res;
    QAction *ac = nullptr;
    QHash<QString,QVariant> data;

    // ---------- Text extractors

    QList<QAction *> pixivActions;
    QUrl pixivUrl = pageUrl;
    if (!pixivUrl.host().contains(QSL("pixiv.net")))
        pixivUrl.clear();
    pixivUrl.setFragment(QString());

    QString pixivId;
    QRegularExpression rxPixivId(QSL("pixiv.net/.*/users/(?<userID>\\d+)"));
    auto mchPixivId = rxPixivId.match(pixivUrl.toString());
    if (mchPixivId.hasMatch())
        pixivId = mchPixivId.captured(QSL("userID"));

    if (pixivUrl.isValid() && pixivUrl.toString().contains(
             QSL("pixiv.net/novel/show.php"), Qt::CaseInsensitive)) {
        ac = new QAction(tr("Extract pixiv novel in new background tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixiv");
        data[QSL("url")] = pixivUrl;
        data[QSL("title")] = title;
        data[QSL("focus")] = false;
        data[QSL("translate")] = false;
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        ac = new QAction(tr("Extract pixiv novel in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixiv");
        data[QSL("url")] = pixivUrl;
        data[QSL("title")] = title;
        data[QSL("focus")] = true;
        data[QSL("translate")] = false;
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        ac = new QAction(tr("Extract pixiv novel in new background tab and translate"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixiv");
        data[QSL("url")] = pixivUrl;
        data[QSL("title")] = title;
        data[QSL("focus")] = false;
        data[QSL("translate")] = true;
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);
    }


    if (pixivUrl.isValid() && !pixivId.isEmpty()) {

        if (!res.isEmpty()) {
            ac = new QAction(menu);
            ac->setSeparator(true);
            res.append(ac);
        }

        ac = new QAction(tr("Extract novel list for author in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivList");
        data[QSL("id")] = pixivId;
        data[QSL("mode")] = QSL("work");
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        ac = new QAction(tr("Extract novel bookmarks list for author in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivList");
        data[QSL("id")] = pixivId;
        data[QSL("mode")] = QSL("bookmarks");
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);
    }

    QList<QAction *> fanboxActions;
    QUrl fanboxUrl = pageUrl;
    if (!fanboxUrl.host().contains(QSL("fanbox.cc")))
        fanboxUrl.clear();
    fanboxUrl.setFragment(QString());

    int fanboxPostId = -1;
    QRegularExpression rxFanboxPostId(QSL("fanbox.cc/.*posts/(?<postID>\\d+)"));
    auto mchFanboxPostId = rxFanboxPostId.match(fanboxUrl.toString());
    if (mchFanboxPostId.hasMatch()) {
        bool ok = false;
        fanboxPostId = mchFanboxPostId.captured(QSL("postID")).toInt(&ok);
        if (!ok)
            fanboxPostId = -1;
    }

    if (fanboxPostId>0) {

        if (!res.isEmpty()) {
            ac = new QAction();
            ac->setSeparator(true);
            res.append(ac);
        }

        ac = new QAction(tr("Extract fanbox.cc novel in new background tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("fanbox");
        data[QSL("id")] = fanboxPostId;
        data[QSL("focus")] = false;
        data[QSL("translate")] = false;
        ac->setData(data);
        res.append(ac);
        fanboxActions.append(ac);

        ac = new QAction(tr("Extract fanbox.cc novel in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("fanbox");
        data[QSL("id")] = fanboxPostId;
        data[QSL("focus")] = true;
        data[QSL("translate")] = false;
        ac->setData(data);
        res.append(ac);
        fanboxActions.append(ac);

        ac = new QAction(tr("Extract fanbox.cc novel in new background tab and translate"),menu);
        data.clear();
        data[QSL("type")] = QSL("fanbox");
        data[QSL("id")] = fanboxPostId;
        data[QSL("focus")] = false;
        data[QSL("translate")] = true;
        ac->setData(data);
        res.append(ac);
        fanboxActions.append(ac);
    }


    // ---------- Manga extractors

    QRegularExpression pixivMangaRx(QSL("pixiv.net/.*?artworks/\\d+"),
                                    QRegularExpression::CaseInsensitiveOption);
    if (origin.toString().contains(pixivMangaRx) ||
            pixivUrl.toString().contains(pixivMangaRx)) {

        if (!res.isEmpty()) {
            ac = new QAction();
            ac->setSeparator(true);
            res.append(ac);
        }

        ac = new QAction(tr("Download all images from Pixiv illustration"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivManga");
        if (origin.toString().contains(pixivMangaRx)) {
            data[QSL("url")] = origin;
        } else {
            data[QSL("url")] = pixivUrl;
        }
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);
    }
    if (fanboxPostId>0) {
        if (!res.isEmpty()) {
            ac = new QAction();
            ac->setSeparator(true);
            res.append(ac);
        }
        ac = new QAction(tr("Download all images from fanbox.cc post"),menu);
        data.clear();
        data[QSL("type")] = QSL("fanboxManga");
        data[QSL("id")] = fanboxPostId;
        ac->setData(data);
        res.append(ac);
        fanboxActions.append(ac);
    }

    QList<QAction *> patreonActions;
    QUrl patreonUrl = pageUrl;
    if (!patreonUrl.host().contains(QSL("patreon.com")))
        patreonUrl.clear();
    patreonUrl.setFragment(QString());
    if (patreonUrl.isValid()) {
        if (!res.isEmpty()) {
            ac = new QAction();
            ac->setSeparator(true);
            res.append(ac);
        }

        ac = new QAction(tr("Download all images from Patreon post"),menu);
        data.clear();
        data[QSL("type")] = QSL("patreonManga");
        data[QSL("url")] = patreonUrl;
        data[QSL("html")] = QSL("need");
        ac->setData(data);
        res.append(ac);
        patreonActions.append(ac);

        ac = new QAction(tr("Download all attachments from Patreon post"));
        data.clear();
        data[QSL("type")] = QSL("patreonAttachments");
        data[QSL("url")] = patreonUrl;
        data[QSL("html")] = QSL("need");
        ac->setData(data);
        res.append(ac);
        patreonActions.append(ac);
    }


    // ------------- Favicon loading

    if (!pixivActions.isEmpty()) {
        QUrl pixivIconUrl(QSL("http://www.pixiv.net/favicon.ico"));
        if (pixivUrl.isValid())
            pixivIconUrl.setScheme(pixivUrl.scheme());
        auto *fl = new CFaviconLoader(snv,pixivIconUrl);
        connect(fl,&CFaviconLoader::finished,fl,&CFaviconLoader::deleteLater);
        connect(fl,&CFaviconLoader::gotIcon, menu, [pixivActions](const QIcon& icon){
            for (auto * const ac : qAsConst(pixivActions)) {
                ac->setIcon(icon);
            }
        });
        fl->queryStart(false);
    }
    if (!fanboxActions.isEmpty()) {
        QUrl fanboxIconUrl(QSL("https://www.fanbox.cc/favicon.ico"));
        if (fanboxUrl.isValid())
            fanboxIconUrl.setScheme(fanboxUrl.scheme());
        auto *fl = new CFaviconLoader(snv,fanboxIconUrl);
        connect(fl,&CFaviconLoader::finished,fl,&CFaviconLoader::deleteLater);
        connect(fl,&CFaviconLoader::gotIcon, menu, [fanboxActions](const QIcon& icon){
            for (auto * const ac : qAsConst(fanboxActions)) {
                ac->setIcon(icon);
            }
        });
        fl->queryStart(false);
    }
    if (!patreonActions.isEmpty()) {
        QUrl patreonIconUrl(QSL("https://c5.patreon.com/external/favicon/favicon.ico"));
        if (patreonUrl.isValid())
            patreonIconUrl.setScheme(patreonUrl.scheme());
        auto *fl = new CFaviconLoader(snv,patreonIconUrl);
        connect(fl,&CFaviconLoader::finished,fl,&CFaviconLoader::deleteLater);
        connect(fl,&CFaviconLoader::gotIcon, menu, [patreonActions](const QIcon& icon){
            for (auto * const ac : qAsConst(patreonActions)) {
                ac->setIcon(icon);
            }
        });
        fl->queryStart(false);
    }

    if (!res.isEmpty()) {
        ac = new QAction(menu);
        ac->setSeparator(true);
        res.append(ac);
    }

    return res;
}

CAbstractExtractor *CAbstractExtractor::extractorFactory(const QVariant &data, CSnippetViewer* snv)
{
    CAbstractExtractor *res = nullptr;

    const auto hash = data.toHash();
    const QString type = hash.value(QSL("type")).toString();

    if (type == QSL("pixiv")) {
        res = new CPixivNovelExtractor(nullptr,snv);
        (qobject_cast<CPixivNovelExtractor *>(res))->setParams(
                    hash.value(QSL("url")).toUrl(),
                    hash.value(QSL("title")).toString(),
                    hash.value(QSL("translate")).toBool(),
                    hash.value(QSL("focus")).toBool());

    } else if (type == QSL("pixivList")) {
        res = new CPixivIndexExtractor(nullptr,snv);
        (qobject_cast<CPixivIndexExtractor *>(res))->setParams(
                    hash.value(QSL("id")).toString(),
                    (hash.value(QSL("mode")).toString() == QSL("work")) ?
                        CPixivIndexExtractor::WorkIndex : CPixivIndexExtractor::BookmarksIndex);

    } else if (type == QSL("fanbox")) {
        res = new CFanboxExtractor(nullptr,snv);
        (qobject_cast<CFanboxExtractor *>(res))->setParams(
                    hash.value(QSL("id")).toInt(),
                    hash.value(QSL("translate")).toBool(),
                    hash.value(QSL("focus")).toBool(),
                    false);

    } else if (type == QSL("pixivManga")) {
        res = new CPixivNovelExtractor(nullptr,snv);
        (qobject_cast<CPixivNovelExtractor *>(res))->setMangaOrigin(
                    hash.value(QSL("url")).toUrl());

    } else if (type == QSL("fanboxManga")) {
        res = new CFanboxExtractor(nullptr,snv);
        (qobject_cast<CFanboxExtractor *>(res))->setParams(
                    hash.value(QSL("id")).toInt(),
                    false,
                    false,
                    true);

    } else if (type == QSL("patreonManga")) {
        res = new CPatreonExtractor(nullptr,snv);
        (qobject_cast<CPatreonExtractor *>(res))->setParams(
                    hash.value(QSL("html")).toString(),
                    hash.value(QSL("url")).toUrl(),
                    false);

    } else if (type == QSL("patreonAttachments")) {
        res = new CPatreonExtractor(nullptr,snv);
        (qobject_cast<CPatreonExtractor *>(res))->setParams(
                    hash.value(QSL("html")).toString(),
                    hash.value(QSL("url")).toUrl(),
                    true);

    }

    return res;
}

void CAbstractExtractor::showError(const QString &message)
{
    const QString err = QSL("%1 error: %2").arg(QString::fromLatin1(metaObject()->className()), message);
    qCritical() << err;

    QWidget *w = nullptr;
    QObject *ctx = QApplication::instance();
    if (m_snv) {
        w = m_snv->parentWnd();
        ctx = m_snv;
    }
    QMetaObject::invokeMethod(ctx,[message,w,err](){
        QMessageBox::warning(w,tr("JPReader"),err);
    },Qt::QueuedConnection);

    Q_EMIT finished();
}

void CAbstractExtractor::start()
{
    startMain();
}

void CAbstractExtractor::loadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    QString msg(QSL("Unable to load from site."));

    auto *rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl)
        msg.append(QSL(" %1").arg(rpl->errorString()));

    showError(msg);
}
