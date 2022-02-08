#include <QMessageBox>
#include <QThread>
#include "abstractextractor.h"
#include "global/control.h"
#include "mainwindow.h"

#include "fanboxextractor.h"
#include "patreonextractor.h"
#include "pixivindexextractor.h"
#include "pixivnovelextractor.h"
#include "deviantartextractor.h"

namespace CDefaults {
const int extractorCreationInterlockMS = 1000;
}

CAbstractExtractor::CAbstractExtractor(QObject *parent)
    : CAbstractThreadWorker(parent)
{
}


QList<QAction *> CAbstractExtractor::addMenuActions(const QUrl &pageUrl, const QUrl &origin,
                                                    const QString &title, QMenu *menu,
                                                    QObject *workersParent, bool skipHtmlParserActions)
{
    QList<QAction *> res;
    QAction *ac = nullptr;
    QHash<QString,QVariant> data;

    // ---------- Text extractors

    const bool showAltTranslator = (gSet->settings()->previousTranslatorEngine !=
            gSet->settings()->translatorEngine);
    const QString altTranName = CStructures::translationEngines().value(gSet->settings()->previousTranslatorEngine);

    QList<QAction *> pixivActions;
    QUrl pixivUrl = pageUrl;
    if (!pixivUrl.host().contains(QSL("pixiv.net")))
        pixivUrl.clear();
    pixivUrl.setFragment(QString());

    QString pixivId;
    QRegularExpression rxPixivId(QSL("pixiv.net/(.*/)?users/(?<userID>\\d+)"));
    auto mchPixivId = rxPixivId.match(pixivUrl.toString());
    if (mchPixivId.hasMatch())
        pixivId = mchPixivId.captured(QSL("userID"));

    QString pixivTag;
    QRegularExpression rxPixivTag(QSL("pixiv.net/(.*/)?tags/(?<tag>\\S+)/novels"));
    auto mchPixivTag = rxPixivTag.match(pixivUrl.toString());
    if (mchPixivTag.hasMatch())
        pixivTag = mchPixivTag.captured(QSL("tag"));

    if (pixivUrl.isValid() && pixivUrl.toString().contains(
             QSL("pixiv.net/novel/show.php"), Qt::CaseInsensitive)) {
        ac = new QAction(tr("Extract pixiv novel in new background tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixiv");
        data[QSL("url")] = pixivUrl;
        data[QSL("title")] = title;
        data[QSL("focus")] = false;
        data[QSL("translate")] = false;
        data[QSL("altTranslate")] = false;
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
        data[QSL("altTranslate")] = false;
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        ac = new QAction(tr("Translate pixiv novel in new background tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixiv");
        data[QSL("url")] = pixivUrl;
        data[QSL("title")] = title;
        data[QSL("focus")] = false;
        data[QSL("translate")] = true;
        data[QSL("altTranslate")] = false;
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        if (showAltTranslator) {
            ac = new QAction(tr("Translate pixiv novel in new background tab (%1)")
                             .arg(altTranName),menu);
            data.clear();
            data[QSL("type")] = QSL("pixiv");
            data[QSL("url")] = pixivUrl;
            data[QSL("title")] = title;
            data[QSL("focus")] = false;
            data[QSL("translate")] = true;
            data[QSL("altTranslate")] = true;
            ac->setData(data);
            res.append(ac);
            pixivActions.append(ac);
        }
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
        data[QSL("base")] = QSL("novel");
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        ac = new QAction(tr("Extract novel bookmarks list for author in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivList");
        data[QSL("id")] = pixivId;
        data[QSL("mode")] = QSL("bookmarks");
        data[QSL("base")] = QSL("novel");
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        ac = new QAction(menu);
        ac->setSeparator(true);
        pixivActions.append(ac);

        ac = new QAction(tr("Extract artworks list for author in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivList");
        data[QSL("id")] = pixivId;
        data[QSL("mode")] = QSL("work");
        data[QSL("base")] = QSL("artwork");
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        ac = new QAction(tr("Extract artworks bookmarks list for author in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivList");
        data[QSL("id")] = pixivId;
        data[QSL("mode")] = QSL("bookmarks");
        data[QSL("base")] = QSL("artwork");
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);
    }

    if (pixivUrl.isValid() && !pixivTag.isEmpty()) {

        if (!res.isEmpty()) {
            ac = new QAction(menu);
            ac->setSeparator(true);
            res.append(ac);
        }

        ac = new QAction(tr("Extract novel tag search list in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivList");
        data[QSL("id")] = pixivTag;
        data[QSL("mode")] = QSL("tagSearch");
        data[QSL("base")] = QSL("novel");
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        ac = new QAction(tr("Extract artwork tag search list in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivList");
        data[QSL("id")] = pixivTag;
        data[QSL("mode")] = QSL("tagSearch");
        data[QSL("base")] = QSL("artwork");
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

        ac = new QAction(tr("Extract fanbox novel in new background tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("fanbox");
        data[QSL("id")] = fanboxPostId;
        data[QSL("focus")] = false;
        data[QSL("translate")] = false;
        data[QSL("altTranslate")] = false;
        ac->setData(data);
        res.append(ac);
        fanboxActions.append(ac);

        ac = new QAction(tr("Extract fanbox novel in new tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("fanbox");
        data[QSL("id")] = fanboxPostId;
        data[QSL("focus")] = true;
        data[QSL("translate")] = false;
        data[QSL("altTranslate")] = false;
        ac->setData(data);
        res.append(ac);
        fanboxActions.append(ac);

        ac = new QAction(tr("Translate fanbox novel in new background tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("fanbox");
        data[QSL("id")] = fanboxPostId;
        data[QSL("focus")] = false;
        data[QSL("translate")] = true;
        data[QSL("altTranslate")] = false;
        ac->setData(data);
        res.append(ac);
        fanboxActions.append(ac);

        if (showAltTranslator) {
            ac = new QAction(tr("Translate fanbox novel in new background tab (%1)")
                             .arg(altTranName),menu);
            data.clear();
            data[QSL("type")] = QSL("fanbox");
            data[QSL("id")] = fanboxPostId;
            data[QSL("focus")] = false;
            data[QSL("translate")] = true;
            data[QSL("altTranslate")] = true;
            ac->setData(data);
            res.append(ac);
            fanboxActions.append(ac);
        }
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

        ac = new QAction(tr("Load Pixiv illustration to background viewer tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivMangaView");
        data[QSL("focus")] = false;
        if (origin.toString().contains(pixivMangaRx)) {
            data[QSL("url")] = origin;
        } else {
            data[QSL("url")] = pixivUrl;
        }
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

        ac = new QAction(tr("Load Pixiv illustration to new viewer tab"),menu);
        data.clear();
        data[QSL("type")] = QSL("pixivMangaView");
        data[QSL("focus")] = true;
        if (origin.toString().contains(pixivMangaRx)) {
            data[QSL("url")] = origin;
        } else {
            data[QSL("url")] = pixivUrl;
        }
        ac->setData(data);
        res.append(ac);
        pixivActions.append(ac);

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
    if (patreonUrl.isValid() && !skipHtmlParserActions) {
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

    QRegularExpression deviantartGalleryRx(QSL("deviantart.com/(?<userID>[^/]+)/gallery"
                                               "(?:/(?<folderID>\\d+)/(?<folderName>[^/\\s]+))?"),
                                           QRegularExpression::CaseInsensitiveOption);
    QList<QAction *> deviantartActions;
    QUrl deviantartUrl = pageUrl;
    if (!deviantartUrl.toString().contains(deviantartGalleryRx))
        deviantartUrl.clear();
    deviantartUrl.setFragment(QString());

    QString deviantUserID;
    QString deviantFolderID;
    QString deviantFolderName;
    auto mchDeviantArt = deviantartGalleryRx.match(deviantartUrl.toString());
    if (mchDeviantArt.hasMatch()) {
        deviantUserID = mchDeviantArt.captured(QSL("userID"));
        deviantFolderID = mchDeviantArt.captured(QSL("folderID"));
        deviantFolderName = mchDeviantArt.captured(QSL("folderName"));
    }

    if (deviantartUrl.isValid() && !deviantUserID.isEmpty()) {
        if (!res.isEmpty()) {
            ac = new QAction();
            ac->setSeparator(true);
            res.append(ac);
        }

        data.clear();
        data[QSL("type")] = QSL("deviantartGallery");
        data[QSL("userID")] = deviantUserID;
        data[QSL("folderID")] = deviantFolderID;
        data[QSL("folderName")] = deviantFolderName;

        if (deviantFolderName.isEmpty())
            deviantFolderName = QSL("All");
        ac = new QAction(tr("Download all images from Deviantart \"%1\" gallery")
                         .arg(deviantFolderName),menu);
        ac->setData(data);
        res.append(ac);
        deviantartActions.append(ac);
    }

    // ------------- Favicon loading

    if (!pixivActions.isEmpty()) {
        QUrl pixivIconUrl(QSL("http://www.pixiv.net/favicon.ico"));
        if (pixivUrl.isValid())
            pixivIconUrl.setScheme(pixivUrl.scheme());
        auto *fl = new CFaviconLoader(workersParent,pixivIconUrl);
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
        auto *fl = new CFaviconLoader(workersParent,fanboxIconUrl);
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
        auto *fl = new CFaviconLoader(workersParent,patreonIconUrl);
        connect(fl,&CFaviconLoader::finished,fl,&CFaviconLoader::deleteLater);
        connect(fl,&CFaviconLoader::gotIcon, menu, [patreonActions](const QIcon& icon){
            for (auto * const ac : qAsConst(patreonActions)) {
                ac->setIcon(icon);
            }
        });
        fl->queryStart(false);
    }
    if (!deviantartActions.isEmpty()) {
        QUrl deviantartIconUrl(QSL("https://st.deviantart.net/eclipse/icons/da_favicon_v2.ico"));
        if (deviantartUrl.isValid())
            deviantartIconUrl.setScheme(deviantartUrl.scheme());
        auto *fl = new CFaviconLoader(workersParent,deviantartIconUrl);
        connect(fl,&CFaviconLoader::finished,fl,&CFaviconLoader::deleteLater);
        connect(fl,&CFaviconLoader::gotIcon, menu, [deviantartActions](const QIcon& icon){
            for (auto * const ac : qAsConst(deviantartActions)) {
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

CAbstractExtractor *CAbstractExtractor::extractorFactory(const QVariant &data, QWidget* parentWidget)
{
    static QMutex lockMutex;
    QMutexLocker locker(&lockMutex);

    CAbstractExtractor *res = nullptr;
    static QElapsedTimer lockTimer;
    if (lockTimer.isValid()) {
        if (lockTimer.elapsed() < CDefaults::extractorCreationInterlockMS)
            return res;
    }
    lockTimer.restart();

    const auto hash = data.toHash();
    const QString type = hash.value(QSL("type")).toString();

    if (type == QSL("pixiv")) {
        res = new CPixivNovelExtractor(nullptr);
        (qobject_cast<CPixivNovelExtractor *>(res))->setParams(
                    hash.value(QSL("url")).toUrl(),
                    hash.value(QSL("title")).toString(),
                    hash.value(QSL("translate")).toBool(),
                    hash.value(QSL("altTranslate")).toBool(),
                    hash.value(QSL("focus")).toBool());

    } else if (type == QSL("pixivList")) {
        static int maxCount = 0;
        static bool originalsOnly = false;
        static QDate dateFrom;
        static QDate dateTo;
        static QString languageCode;
        static CPixivIndexExtractor::NovelSearchLength nsl = CPixivIndexExtractor::nslDefault;
        static CPixivIndexExtractor::SearchRating nsr = CPixivIndexExtractor::srAll;
        static CPixivIndexExtractor::TagSearchMode tsm = CPixivIndexExtractor::tsmTagFull;

        QString keywords = hash.value(QSL("id")).toString();
        CPixivIndexExtractor::IndexMode mode = CPixivIndexExtractor::imWorkIndex;
        if (hash.value(QSL("mode")).toString() == QSL("work")) {
            mode = CPixivIndexExtractor::imWorkIndex;
        } else if (hash.value(QSL("mode")).toString() == QSL("bookmarks")) {
            mode = CPixivIndexExtractor::imBookmarksIndex;
        } else if (hash.value(QSL("mode")).toString() == QSL("tagSearch")) {
            mode = CPixivIndexExtractor::imTagSearchIndex;
        } else {
            return res;
        }
        CPixivIndexExtractor::ExtractorMode exMode = CPixivIndexExtractor::emNovels;
        if (hash.value(QSL("base")).toString() == QSL("artwork"))
            exMode = CPixivIndexExtractor::emArtworks;

        if (CPixivIndexExtractor::extractorLimitsDialog(parentWidget,tr("Pixiv index filter"),
                                  tr("Extractor filter"),(mode == CPixivIndexExtractor::imTagSearchIndex),
                                  maxCount,dateFrom,dateTo,keywords,tsm,originalsOnly,languageCode,nsl,nsr))
        {
            res = new CPixivIndexExtractor(nullptr);
            (qobject_cast<CPixivIndexExtractor *>(res))->setParams(
                        keywords,
                        exMode,
                        mode,
                        maxCount,
                        dateFrom,
                        dateTo,
                        tsm,
                        originalsOnly,
                        languageCode,
                        nsl,
                        nsr);
        }

    } else if (type == QSL("fanbox")) {
        res = new CFanboxExtractor(nullptr);
        (qobject_cast<CFanboxExtractor *>(res))->setParams(
                    hash.value(QSL("id")).toInt(),
                    hash.value(QSL("translate")).toBool(),
                    hash.value(QSL("altTranslate")).toBool(),
                    hash.value(QSL("focus")).toBool(),
                    false);

    } else if (type == QSL("pixivManga")) {
        res = new CPixivNovelExtractor(nullptr);
        (qobject_cast<CPixivNovelExtractor *>(res))->setMangaParams(
                    hash.value(QSL("url")).toUrl(),
                    false,
                    false);

    } else if (type == QSL("pixivMangaView")) {
        res = new CPixivNovelExtractor(nullptr);
        (qobject_cast<CPixivNovelExtractor *>(res))->setMangaParams(
                    hash.value(QSL("url")).toUrl(),
                    true,
                    hash.value(QSL("focus")).toBool());

    } else if (type == QSL("fanboxManga")) {
        res = new CFanboxExtractor(nullptr);
        (qobject_cast<CFanboxExtractor *>(res))->setParams(
                    hash.value(QSL("id")).toInt(),
                    false,
                    false,
                    false,
                    true);

    } else if (type == QSL("patreonManga")) {
        res = new CPatreonExtractor(nullptr);
        (qobject_cast<CPatreonExtractor *>(res))->setParams(
                    hash.value(QSL("html")).toString(),
                    hash.value(QSL("url")).toUrl(),
                    false);

    } else if (type == QSL("patreonAttachments")) {
        res = new CPatreonExtractor(nullptr);
        (qobject_cast<CPatreonExtractor *>(res))->setParams(
                    hash.value(QSL("html")).toString(),
                    hash.value(QSL("url")).toUrl(),
                    true);

    } else if (type == QSL("deviantartGallery")) {
        res = new CDeviantartExtractor(nullptr);
        (qobject_cast<CDeviantartExtractor *>(res))->setParams(
                    hash.value(QSL("userID")).toString(),
                    hash.value(QSL("folderID")).toString(),
                    hash.value(QSL("folderName")).toString());

    }

    return res;
}

void CAbstractExtractor::showError(const QString &message)
{
    const QString err = QSL("%1 error: %2").arg(QString::fromLatin1(metaObject()->className()), message);
    qCritical() << err;
    Q_EMIT errorOccured(err);
    Q_EMIT finished();
}

void CAbstractExtractor::loadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));

    QString msg(QSL("Unable to load from site."));
    if (rpl)
        msg.append(QSL(" %1").arg(rpl->errorString()));

    showError(msg);
}

QJsonDocument CAbstractExtractor::parseJsonSubDocument(const QByteArray& source, const QRegularExpression& start)
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
