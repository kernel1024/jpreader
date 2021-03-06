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
private:
    bool m_extractAttachments { false };
    QString m_html;
    QUrl m_origin;

public:
    CPatreonExtractor(QObject *parent, QWidget *parentWidget);
    void setParams(const QString& pageHtml, const QUrl& origin, bool extractAttachments);
    QString workerDescription() const override;

protected:
    void startMain() override;

};

#endif // PATREONEXTRACTOR_H
