#ifndef PIXIVNOVELEXTRACTOR_H
#define PIXIVNOVELEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QStringList>
#include <QAtomicInteger>
#include <QMutex>
#include <QUrl>
#include <QJsonDocument>
#include <snviewer.h>
#include <structures.h>

class CPixivNovelExtractor : public QObject
{
    Q_OBJECT
private:
    QString m_title;
    bool m_translate { false };
    bool m_focus { false };
    QAtomicInteger<int> m_worksPageLoad, m_worksImgFetch;
    CSnippetViewer* m_snv { nullptr };
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
    QJsonDocument parseJsonSubDocument(const QByteArray &source, const QRegularExpression &start);
    QStringList parseJsonIllustPage(const QString &html, const QUrl& origin, QString *id = nullptr);

public:
    explicit CPixivNovelExtractor(QObject *parent = nullptr);
    void setParams(CSnippetViewer* viewer, const QUrl& source, const QString& title,
                   bool translate, bool focus);
    void setMangaOrigin(CSnippetViewer *viewer, const QUrl& origin);

Q_SIGNALS:
    void novelReady(const QString& html, bool focus, bool translate);
    void mangaReady(const QStringList& urls, const QString &id, const QUrl &origin);
    void finished();

public Q_SLOTS:
    void start();
    void startManga();
    void novelLoadFinished();
    void loadError(QNetworkReply::NetworkError error);

private Q_SLOTS:
    void subLoadFinished();
    void subWorkFinished();
    void subImageFinished();

};

#endif // PIXIVNOVELEXTRACTOR_H
