#ifndef PATREONEXTRACTOR_H
#define PATREONEXTRACTOR_H

#include <QObject>
#include <QPair>
#include <QString>
#include <QUrl>
#include "snviewer.h"
#include "structures.h"

class CPatreonExtractor : public QObject
{
    Q_OBJECT
private:
    bool m_extractAttachments { false };
    QString m_html;
    QUrl m_origin;
    CSnippetViewer* m_snv { nullptr };

    void showError(const QString &message);

public:
    explicit CPatreonExtractor(QObject *parent = nullptr);
    void setParams(CSnippetViewer* viewer, const QString& pageHtml, const QUrl& origin, bool extractAttachments);

Q_SIGNALS:
    void mangaReady(const QVector<CUrlWithName>& urls, const QString& id, const QUrl &origin);
    void finished();

public Q_SLOTS:
    void start();

};

#endif // PATREONEXTRACTOR_H
