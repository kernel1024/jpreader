#ifndef FANBOXEXTRACTOR_H
#define FANBOXEXTRACTOR_H

#include <QObject>
#include <QNetworkReply>
#include <QMutex>
#include "abstractextractor.h"
#include "../structures.h"

class CFanboxExtractor : public CAbstractExtractor
{
    Q_OBJECT
private:
    int m_postId { -1 };
    bool m_translate { false };
    bool m_focus { false };
    bool m_isManga { false };
    QVector<CUrlWithName> m_illustMap;
    QAtomicInteger<int> m_worksIllustFetch;
    QMutex m_illustMutex;

    QString m_title;
    QString m_text;
    QString m_authorId;
    QString m_postNum;

public:
    CFanboxExtractor(QObject *parent, QWidget *parentWidget);
    void setParams(int postId, bool translate, bool focus, bool isManga);

protected:
    void startMain() override;

private Q_SLOTS:
    void subImageFinished();
    void pageLoadFinished();

};

#endif // FANBOXEXTRACTOR_H
