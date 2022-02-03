#ifndef ABSTRACTEXTRACTOR_H
#define ABSTRACTEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QVariant>
#include <QJsonDocument>
#include <QByteArray>
#include <QRegularExpression>
#include <QAction>
#include "global/structures.h"
#include "abstractthreadworker.h"

class CAbstractExtractor : public CAbstractThreadWorker
{
    Q_OBJECT
public:
    CAbstractExtractor(QObject *parent);

    static QList<QAction *> addMenuActions(const QUrl& pageUrl, const QUrl &origin,
                                           const QString &title, QMenu *menu, QObject *workersParent,
                                           bool skipHtmlParserActions);
    static CAbstractExtractor* extractorFactory(const QVariant &data, QWidget *parentWidget);

protected:
    void showError(const QString &message);
    QJsonDocument parseJsonSubDocument(const QByteArray &source, const QRegularExpression &start);

Q_SIGNALS:
    void novelReady(const QString& html, bool focus, bool translate, bool alternateTranslate);
    void mangaReady(const QVector<CUrlWithName>& urls, const QString &id, const QUrl &origin, bool useViewer);

protected Q_SLOTS:
    void loadError(QNetworkReply::NetworkError error);

};

#endif // ABSTRACTEXTRACTOR_H
