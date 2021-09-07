#ifndef CDEVIANTARTEXTRACTOR_H
#define CDEVIANTARTEXTRACTOR_H

#include <QObject>
#include <QJsonObject>
#include "abstractextractor.h"

class CDeviantartExtractor : public CAbstractExtractor
{
    Q_OBJECT
private:
    QString m_userID;
    QString m_folderID;
    QString m_folderName;
    QVector<QJsonObject> m_list;

    void finalizeGallery();

public:
    CDeviantartExtractor(QObject *parent);

    void setParams(const QString& userID, const QString& folderID, const QString& folderName);
    QString workerDescription() const override;

protected:
    void startMain() override;

private Q_SLOTS:
    void galleryAjax();

};

#endif // CDEVIANTARTEXTRACTOR_H
