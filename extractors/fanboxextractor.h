#ifndef FANBOXEXTRACTOR_H
#define FANBOXEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QMutex>
#include "abstractextractor.h"
#include "global/structures.h"

class CFanboxExtractor : public CAbstractExtractor
{
    Q_OBJECT
    Q_DISABLE_COPY(CFanboxExtractor)
private:
    int m_postId { -1 };
    bool m_translate { false };
    bool m_alternateTranslate { false };
    bool m_focus { false };
    bool m_downloadNovel { false };
    bool m_isManga { false };
    QVector<CUrlWithName> m_illustMap;
    QAtomicInteger<int> m_worksIllustFetch;
    QMutex m_illustMutex;

    QString m_title;
    QString m_text;
    QString m_authorId;
    QString m_postNum;
    CStringHash m_auxInfo;

public:
    explicit CFanboxExtractor(QObject *parent);
    ~CFanboxExtractor() override = default;
    void setParams(int postId, bool translate, bool alternateTranslate, bool focus,
                   bool isManga, bool novelDownload, const CStringHash &auxInfo);
    QString workerDescription() const override;

protected:
    void startMain() override;

private Q_SLOTS:
    void subImageFinished();
    void pageLoadFinished();

};

#endif // FANBOXEXTRACTOR_H
