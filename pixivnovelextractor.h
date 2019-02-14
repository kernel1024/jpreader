#ifndef PIXIVNOVELEXTRACTOR_H
#define PIXIVNOVELEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QStringList>
#include <QAtomicInteger>
#include <QMutex>
#include <snviewer.h>

class CPixivNovelExtractor : public QObject
{
    Q_OBJECT
private:
    QString m_title;
    bool m_translate;
    bool m_focus;
    QAtomicInteger<int> m_works;
    CSnippetViewer* m_snv;
    QMutex m_imgMutex;
    QHash<QString,QSet<int> > m_imgList;
    QHash<QString,QString> m_imgUrls;

    void handleImages(const QStringList& imgs, const QUrl &referer);
    void syncSubWorks(QString &html);

public:
    explicit CPixivNovelExtractor(QObject *parent = nullptr);
    void setParams(CSnippetViewer* viewer, const QString& title,
                   bool translate, bool focus);

signals:
    void novelReady(const QString& html, bool focus, bool translate);

public slots:
    void novelLoadFinished();
    void novelLoadError(QNetworkReply::NetworkError error);

private slots:
    void subLoadError(QNetworkReply::NetworkError error);
    void subLoadFinished();

};

#endif // PIXIVNOVELEXTRACTOR_H
