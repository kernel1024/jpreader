#ifndef PATREONEXTRACTOR_H
#define PATREONEXTRACTOR_H

#include <QObject>
#include <QPair>
#include <QString>
#include <QUrl>
#include "abstractextractor.h"
#include "global/structures.h"

class CPatreonExtractor : public CAbstractExtractor
{
    Q_OBJECT
    Q_DISABLE_COPY(CPatreonExtractor)
private:
    bool m_extractAttachments { false };
    QString m_html;
    QUrl m_origin;

public:
    explicit CPatreonExtractor(QObject *parent);
    ~CPatreonExtractor() override = default;
    void setParams(const QString& pageHtml, const QUrl& origin, bool extractAttachments);
    QString workerDescription() const override;

protected:
    void startMain() override;

};

#endif // PATREONEXTRACTOR_H
