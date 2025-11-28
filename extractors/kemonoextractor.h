#ifndef KEMONOEXTRACTOR_H
#define KEMONOEXTRACTOR_H

#include <QObject>
#include <QMutex>

#include "abstractextractor.h"

class CKemonoExtractor : public CAbstractExtractor
{
    Q_OBJECT
    Q_DISABLE_COPY(CKemonoExtractor)
private:
    int m_postId { -1 };
    int m_authorId { -1 };
    bool m_translate { false };
    bool m_alternateTranslate { false };
    bool m_focus { false };
    bool m_download { false };
    bool m_isManga { false };
    QMutex m_authorsMutex;

    QString m_sourceRepo;
    QString m_title;
    QString m_author;
    QString m_text;
    QString m_postNum;
    CStringHash m_auxInfo;

public:
    explicit CKemonoExtractor(QObject *parent);
    ~CKemonoExtractor() override = default;
    void setParams(const QString& sourceRepo, int authorId, int postId, bool translate, bool alternateTranslate, bool focus,
                   bool isManga, bool download, const CStringHash &auxInfo);
    QString workerDescription() const override;

protected:
    void startMain() override;

private Q_SLOTS:
    void pageLoadFinished();

};

#endif // KEMONOEXTRACTOR_H
