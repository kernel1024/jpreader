#ifndef PIXIVINDEXEXTRACTOR_H
#define PIXIVINDEXEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QJsonObject>
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
    QVector<QJsonObject> m_list;
    QStringList m_ids;
    IndexMode m_indexMode { UndefinedIndex };

    void showError(const QString& message);
    void fetchNovelsInfo();
    QString makeNovelInfoBlock(CStringHash* authors,
                               const QString& workId, const QString& workImgUrl,
                               const QString& title, int length,
                               const QString& author, const QString& authorId,
                               const QStringList& tags, const QString& description,
                               const QDateTime &creationDate, const QString &seriesTitle,
                               const QString &seriesId);
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
