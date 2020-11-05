#ifndef CABSTRACTEXTRACTOR_H
#define CABSTRACTEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QVariant>
#include <QJsonDocument>
#include <QByteArray>
#include <QRegularExpression>
#include <QAction>
#include "../structures.h"

class CAbstractExtractor : public QObject
{
    Q_OBJECT
private:
    QAtomicInteger<bool> m_abortFlag;
    QAtomicInteger<bool> m_abortedFinished;
    QWidget* m_parentWidget { nullptr };
    qint64 m_loadedTotalSize { 0L };
    qint64 m_loadedRequestCount { 0L };

public:
    CAbstractExtractor(QObject *parent, QWidget *parentWidget);

    QWidget *parentWidget() const;

    static QList<QAction *> addMenuActions(const QUrl& pageUrl, const QUrl &origin,
                                           const QString &title, QMenu *menu, QObject *workersParent,
                                           bool skipHtmlParserActions);
    static CAbstractExtractor* extractorFactory(const QVariant &data, QWidget *parentWidget);
    static CAbstractExtractor* extractorWorkerFactory(const QVariant &data, QWidget *parentWidget);

protected:
    virtual void startMain() = 0;
    void showError(const QString &message);
    QJsonDocument parseJsonSubDocument(const QByteArray &source, const QRegularExpression &start);
    void addLoadedRequest(qint64 size);
    bool exitIfAborted();

Q_SIGNALS:
    void novelReady(const QString& html, bool focus, bool translate);
    void mangaReady(const QVector<CUrlWithName>& urls, const QString &id, const QUrl &origin);
    void finished();
    void dataLoaded();

public Q_SLOTS:
    void start();
    void abort();

protected Q_SLOTS:
    void loadError(QNetworkReply::NetworkError error);

};

#endif // CABSTRACTEXTRACTOR_H
