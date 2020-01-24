#ifndef CHTMLIMAGESEXTRACTOR_H
#define CHTMLIMAGESEXTRACTOR_H

#include <QObject>
#include <QMutex>
#include <QNetworkReply>
#include "translator.h"


class CHtmlImagesExtractor : public QObject
{
    Q_OBJECT
private:
    bool m_translate { false };
    bool m_focus { false };
    QAtomicInteger<int> m_worksImgFetch;
    QList<CHTMLAttributesHash *> m_imgUrls;
    QUrl m_origin;
    QString m_html;
    CHTMLNode m_doc;

    void handleImages();
    void finalizeHtml();
    void examineNode(CHTMLNode & node);

public:
    explicit CHtmlImagesExtractor(QObject *parent = nullptr);
    ~CHtmlImagesExtractor() = default;
    void setParams(const QString &source, const QUrl &origin,
                   bool translate, bool focus);

Q_SIGNALS:
    void htmlReady(const QString& html, bool focus, bool translate);
    void finished();

public Q_SLOTS:
    void start();

private Q_SLOTS:
    void subImageFinished(QNetworkReply *rpl, CHTMLAttributesHash *attrs);

};

#endif // CHTMLIMAGESEXTRACTOR_H
