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
    QList<QPointer<CAbstractThreadWorker> > m_workers;
    void takeNextWorker();
    void cleanupWorkers();

public:
    explicit CSequenceScheduler(QObject *parent = nullptr);
    ~CSequenceScheduler() override;
    QString workerDescription() const override;
    void setParams(const QList<QPointer<CAbstractThreadWorker> > &workers);
    int workerWeight() override;

protected:
    void startMain() override;

private Q_SLOTS:
    void subWorkerFinished();
    void subWorkerError(const QString& message);
    void abortSequence();

};

#endif // SEQUENCESCHEDULER_H
