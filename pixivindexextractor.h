#ifndef PIXIVINDEXEXTRACTOR_H
#define PIXIVINDEXEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include "snviewer.h"
#include "structures.h"

class CPixivIndexExtractor : public QObject
{
    Q_OBJECT

public:
    enum IndexMode { UndefinedIndex, WorkIndex, BookmarksIndex };
    Q_ENUM(IndexMode)

    explicit CPixivIndexExtractor(QObject *parent = nullptr);
    void setParams(CSnippetViewer *viewer, const QString& pixivId);

private:

    CSnippetViewer *m_snv { nullptr };
    QString m_authorId;
    QString m_html;
    QStringList m_ids;
    CStringHash m_authors;
    IndexMode m_indexMode { UndefinedIndex };
    int m_infoBlockCount { 0 };

    void showError(const QString& message);
    void fetchNovelsInfo();
    void addNovelInfoBlock(const QString& workId, const QString& workImgUrl,
                           const QString& title, int length,
                           const QString& author, const QString& authorId,
                           const QStringList& tags, const QString& description);
    void finalizeHtml();

Q_SIGNALS:
    void listReady(const QString& html);
    void finished();

private Q_SLOTS:
    void profileAjax();
    void bookmarksAjax();
    void loadError(QNetworkReply::NetworkError error);

public Q_SLOTS:
    void createNovelList();
    void createNovelBookmarksList();
};

#endif // PIXIVINDEXEXTRACTOR_H
