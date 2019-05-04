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
    bool m_translate;
    bool m_focus;
    QAtomicInteger<int> m_worksPageLoad, m_worksImgFetch;
    CSnippetViewer* m_snv;
    QMutex m_imgMutex;
    QHash<QString,CIntList> m_imgList;
    QHash<QString,QString> m_imgUrls;
    QUrl m_origin;
    QString m_novelId;
    QString m_html;

    void handleImages(const QStringList& imgs);
    QString parseJsonNovel(const QString& html, QStringList& tags,
                           QString& author, QString& authorNum, QString& title);
    static QJsonDocument parseJsonSubDocument(const QByteArray &source, const QString &start);

public:
    explicit CPixivNovelExtractor(QObject *parent = nullptr);
    void setParams(CSnippetViewer* viewer, const QString& title,
                   bool translate, bool focus);
    static QStringList parseJsonIllustPage(const QString &html, const QUrl& origin);
    static QStringList parseIllustPage(const QString &html, bool mediumMode = true);

signals:
    void novelReady(const QString& html, bool focus, bool translate);

public slots:
    void novelLoadFinished();
    void novelLoadError(QNetworkReply::NetworkError error);

private slots:
    void subLoadFinished();
    void subWorkFinished();
    void subImageFinished();

};

#endif // PIXIVNOVELEXTRACTOR_H
