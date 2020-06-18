#ifndef FANBOXEXTRACTOR_H
#define FANBOXEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include "snviewer.h"
#include "structures.h"

class CFanboxExtractor : public QObject
{
    Q_OBJECT
private:
    int m_postId { -1 };
    bool m_translate { false };
    bool m_focus { false };
    CSnippetViewer* m_snv { nullptr };

    void showError(const QString &message);

public:
    explicit CFanboxExtractor(QObject *parent = nullptr);
    void setParams(CSnippetViewer* viewer, int postId,
                   bool translate, bool focus);

Q_SIGNALS:
    void novelReady(const QString& html, bool focus, bool translate);
    void mangaReady(const QVector<CUrlWithName>& urls, const QString &id, const QUrl &origin);
    void finished();

public Q_SLOTS:
    void start();
    void pageLoadFinished();
    void loadError(QNetworkReply::NetworkError error);

};

#endif // FANBOXEXTRACTOR_H
