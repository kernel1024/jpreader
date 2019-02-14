#ifndef PIXIVNOVELEXTRACTOR_H
#define PIXIVNOVELEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <snviewer.h>

class CPixivNovelExtractor : public QObject
{
    Q_OBJECT
private:
    QString m_title;
    bool m_translate;
    bool m_focus;
    CSnippetViewer* m_snv;

public:
    explicit CPixivNovelExtractor(QObject *parent = nullptr);
    void setParams(CSnippetViewer* viewer, const QString& title,
                   bool translate, bool focus);

signals:
    void novelReady(const QString& html, bool focus, bool translate);

public slots:
    void novelLoadFinished();
    void novelLoadError(QNetworkReply::NetworkError error);
};

#endif // PIXIVNOVELEXTRACTOR_H
