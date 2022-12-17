#include "sequencescheduler.h"
#include "global/startup.h"
#include "global/control.h"

CSequenceScheduler::CSequenceScheduler(QObject *parent)
    : CAbstractThreadWorker(parent)
{
}

CSequenceScheduler::~CSequenceScheduler()
{
    cleanupWorkers();
}

QString CSequenceScheduler::workerDescription() const
{
    return tr("Sequence scheduler (%1 remains)")
            .arg(m_workers.count());
}

void CSequenceScheduler::setParams(const QList<QPointer<CAbstractThreadWorker> > &workers)
{
    m_workers = workers;
}

int CSequenceScheduler::workerWeight()
{
    return -1;
}

void CSequenceScheduler::startMain()
{
    takeNextWorker();
}

void CSequenceScheduler::subWorkerFinished()
{
    addLoadedRequest(0L);
    takeNextWorker();
}

void CSequenceScheduler::subWorkerError(const QString &message)
{
    auto remainedCount = m_workers.count();
    cleanupWorkers();
    Q_EMIT errorOccured(tr("Sequence scheduler aborted, %1 works completed, %2 remained.\nLast error: %3")
                        .arg(loadedRequestCount()).arg(remainedCount).arg(message));
    Q_EMIT finished();
}

void CSequenceScheduler::abortSequence()
{
    cleanupWorkers();
    Q_EMIT errorOccured(tr("Failed to setup download worker"));
    Q_EMIT finished();
}

void CSequenceScheduler::takeNextWorker()
{
    if (exitIfAborted()) return;

    if (m_workers.isEmpty()) {
        Q_EMIT finished();
        return;
    }

    auto work = m_workers.takeFirst();
    QMetaObject::invokeMethod(gSet,[work,this](){
        if (!gSet->startup()->setupThreadedWorker(work)) {
            work->deleteLater();
            QMetaObject::invokeMethod(this,&CSequenceScheduler::abortSequence,Qt::QueuedConnection);
            return;
        }

        connect(work,&CAbstractThreadWorker::finished,
                this,&CSequenceScheduler::subWorkerFinished,Qt::QueuedConnection);
        connect(work,&CAbstractThreadWorker::errorOccured,
                this,&CSequenceScheduler::subWorkerError,Qt::QueuedConnection);

        QMetaObject::invokeMethod(work,&CAbstractThreadWorker::start,Qt::QueuedConnection);
    },Qt::QueuedConnection);
}

void CSequenceScheduler::cleanupWorkers()
{
    for (const auto& worker : qAsConst(m_workers))
        worker->deleteLater();
    m_workers.clear();
}
