#ifndef CDEVIANTARTEXTRACTOR_H
#define CDEVIANTARTEXTRACTOR_H

#include <QObject>
#include <QJsonObject>
#include "abstractextractor.h"

class CDeviantartExtractor : public CAbstractExtractor
{
    Q_OBJECT
    Q_DISABLE_COPY(CDeviantartExtractor)
private:
    QString m_userID;
    QString m_folderID;
    QString m_folderName;
    QVector<QJsonObject> m_list;

    void finalizeGallery();

public:
    explicit CDeviantartExtractor(QObject *parent);
    ~CDeviantartExtractor() override = default;

    void setParams(const QString& userID, const QString& folderID, const QString& folderName);
    QString workerDescription() const override;

protected:
    void startMain() override;

private Q_SLOTS:
    void galleryAjax();

};

#endif // CDEVIANTARTEXTRACTOR_H
