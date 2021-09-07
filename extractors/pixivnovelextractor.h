#ifndef PIXIVNOVELEXTRACTOR_H
#define PIXIVNOVELEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QStringList>
#include <QAtomicInteger>
#include <QMutex>
#include <QUrl>
#include "abstractextractor.h"
#include "global/structures.h"

class CPixivNovelExtractor : public CAbstractExtractor
{
    Q_OBJECT
private:
    QString m_title;
    bool m_translate { false };
    bool m_alternateTranslate { false };
    bool m_focus { false };
    QAtomicInteger<int> m_worksPageLoad, m_worksImgFetch;
    QMutex m_imgMutex;
    QHash<QString,CIntList> m_imgList;
    QHash<QString,QString> m_imgUrls;
    QUrl m_origin;
    QUrl m_mangaOrigin;
    QUrl m_source;
    QString m_novelId;
    QString m_html;

    void handleImages(const QStringList& imgs, const CStringHash &embImgs, const QUrl &mainReferer);
    void pixivDirectFetchImage(const QUrl &url, const QUrl &referer, const QString &pageId);
    QString parseJsonNovel(const QString& html, QStringList& tags,
                           QString& author, QString& authorNum, QString& title,
                           CStringHash &embeddedImages);
    QVector<CUrlWithName> parseJsonIllustPage(const QString &html, const QUrl& origin,
                                              QString *illustID = nullptr);

public:
    CPixivNovelExtractor(QObject *parent);
    void setParams(const QUrl& source, const QString& title,
                   bool translate, bool alternateTranslate, bool focus);
    void setMangaOrigin(const QUrl& origin);
    QString workerDescription() const override;

protected:
    void startMain() override;

public Q_SLOTS:
    void novelLoadFinished();

private Q_SLOTS:
    void subLoadFinished();
    void subWorkFinished();
    void subImageFinished();

};

#endif // PIXIVNOVELEXTRACTOR_H
