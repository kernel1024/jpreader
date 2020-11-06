#ifndef CHTMLIMAGESEXTRACTOR_H
#define CHTMLIMAGESEXTRACTOR_H

#include <QObject>
#include <QMutex>
#include <QNetworkReply>
#include "../translator.h"
#include "abstractextractor.h"

class CHtmlImagesExtractor : public CAbstractExtractor
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

protected:
    void startMain() override;

public:
    CHtmlImagesExtractor(QObject *parent = nullptr, CSnippetViewer *snv = nullptr);
    ~CHtmlImagesExtractor() = default;
    void setParams(const QString &source, const QUrl &origin,
                   bool translate, bool focus);
    QString workerDescription() const override;

private Q_SLOTS:
    void subImageFinished(QNetworkReply *rpl, CHTMLAttributesHash *attrs);

};

#endif // CHTMLIMAGESEXTRACTOR_H
