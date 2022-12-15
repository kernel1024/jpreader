#ifndef SEQUENCESCHEDULER_H
#define SEQUENCESCHEDULER_H

#include <QObject>
#include <QList>
#include <QPointer>
#include "abstractthreadworker.h"

class CSequenceScheduler : public CAbstractThreadWorker
{
    Q_OBJECT
    Q_DISABLE_COPY(CSequenceScheduler)
private:
    QList<QPointer<CAbstractThreadWorker> > m_works;
    void takeNextWork();

public:
    explicit CSequenceScheduler(QObject *parent = nullptr);
    ~CSequenceScheduler() override;
    QString workerDescription() const override;
    void setParams(const QList<QPointer<CAbstractThreadWorker> > &works);

protected:
    void startMain() override;

public Q_SLOTS:
    void subWorkerFinished();

};

#endif // SEQUENCESCHEDULER_H
