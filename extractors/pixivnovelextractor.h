#ifndef PIXIVNOVELEXTRACTOR_H
#define PIXIVNOVELEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QStringList>
#include <QAtomicInteger>
#include <QMutex>
#include <QUrl>
#include "abstractextractor.h"
#include "../structures.h"

class CPixivNovelExtractor : public CAbstractExtractor
{
    Q_OBJECT
private:
    QString m_title;
    bool m_translate { false };
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

    void handleImages(const QStringList& imgs);
    QString parseJsonNovel(const QString& html, QStringList& tags,
                           QString& author, QString& authorNum, QString& title);
    QVector<CUrlWithName> parseJsonIllustPage(const QString &html, const QUrl& origin, QString *illustID = nullptr);

public:
    CPixivNovelExtractor(QObject *parent, QWidget *parentWidget);
    void setParams(const QUrl& source, const QString& title,
                   bool translate, bool focus);
    void setMangaOrigin(const QUrl& origin);

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
