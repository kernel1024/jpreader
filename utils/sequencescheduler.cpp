#include "sequencescheduler.h"

CSequenceScheduler::CSequenceScheduler(QObject *parent)
    : CAbstractThreadWorker(parent)
{
}

CSequenceScheduler::~CSequenceScheduler()
{
    for (const auto &work : qAsConst(m_works)) {
        if (work)
            work->deleteLater();
    }
}

QString CSequenceScheduler::workerDescription() const
{
    return tr("Sequence scheduler (%1 remains)")
            .arg(m_works.count());
}

void CSequenceScheduler::setParams(const QList<QPointer<CAbstractThreadWorker> > &works)
{
    m_works = works;
}

void CSequenceScheduler::startMain()
{
    takeNextWork();
}

void CSequenceScheduler::subWorkerFinished()
{
    addLoadedRequest(0L);
    takeNextWork();
}

void CSequenceScheduler::takeNextWork()
{
    if (exitIfAborted()) return;

    if (m_works.isEmpty()) {
        Q_EMIT finished();
        return;
    }
    auto work = m_works.takeFirst();

    connect(work,&CAbstractThreadWorker::finished,
            this,&CSequenceScheduler::subWorkerFinished,Qt::QueuedConnection);

    QMetaObject::invokeMethod(work,&CAbstractThreadWorker::start,Qt::QueuedConnection);
}
