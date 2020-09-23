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
    CDeviantartExtractor(QObject *parent, CSnippetViewer *snv);

    void setParams(const QString& userID, const QString& folderID, const QString& folderName);

protected:
    void startMain() override;

private Q_SLOTS:
    void galleryAjax();

};

#endif // CDEVIANTARTEXTRACTOR_H
