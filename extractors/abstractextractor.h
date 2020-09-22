#ifndef CABSTRACTEXTRACTOR_H
#define CABSTRACTEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QVariant>
#include "../snviewer.h"
#include "../structures.h"

class CAbstractExtractor : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer* m_snv { nullptr };

public:
    CAbstractExtractor(QObject *parent = nullptr, CSnippetViewer *snv = nullptr);

    CSnippetViewer *snv() const;

    static QList<QAction *> addMenuActions(const QUrl& pageUrl, const QUrl &origin,
                                           const QString &title, QMenu *menu, CSnippetViewer *snv);
    static CAbstractExtractor* extractorFactory(const QVariant &data, CSnippetViewer *snv);

protected:
    virtual void startMain() = 0;
    void showError(const QString &message);

Q_SIGNALS:
    void novelReady(const QString& html, bool focus, bool translate);
    void mangaReady(const QVector<CUrlWithName>& urls, const QString &id, const QUrl &origin);
    void listReady(const QString& html);
    void finished();

public Q_SLOTS:
    void start();

protected Q_SLOTS:
    void loadError(QNetworkReply::NetworkError error);

};

#endif // CABSTRACTEXTRACTOR_H
