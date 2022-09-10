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
    explicit CAbstractExtractor(QObject *parent);

    static QList<QAction *> addMenuActions(const QUrl& pageUrl, const QUrl &origin,
                                           const QString &title, QMenu *menu, QObject *workersParent,
                                           bool skipHtmlParserActions);
    static CAbstractExtractor* extractorFactory(const QVariant &data, QWidget *parentWidget);

protected:
    void showError(const QString &message);
    QJsonDocument parseJsonSubDocument(const QByteArray &source, const QRegularExpression &start);

Q_SIGNALS:
    void novelReady(const QString& html, bool focus, bool translate, bool alternateTranslate, bool downloadNovel,
                    const CStringHash& info);
    void mangaReady(const QVector<CUrlWithName>& urls, const QString &id, const QUrl &origin, const QString &title,
                    const QString& description, bool useViewer, bool focus, bool originalScale);

protected Q_SLOTS:
    void loadError(QNetworkReply::NetworkError error);

};

#endif // ABSTRACTEXTRACTOR_H
