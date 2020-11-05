#include "abstractthreadworker.h"

CAbstractThreadWorker::CAbstractThreadWorker(QObject *parent) : QObject(parent)
{
}

void CAbstractThreadWorker::start()
{
    resetAbortFlag();
    startMain();
    Q_EMIT started();
}

void CAbstractThreadWorker::abort()
{
    m_abortFlag.storeRelease(true);
}

void CAbstractThreadWorker::addLoadedRequest(qint64 size)
{
    m_loadedTotalSize += size;
    m_loadedRequestCount++;

    Q_EMIT dataLoaded();
}

bool CAbstractThreadWorker::exitIfAborted()
{
    if (m_abortFlag.loadAcquire()) {
        if (m_abortedFinished.testAndSetOrdered(false,true)) {
            Q_EMIT finished();
        }
        return true;
    }
    return false;
}

bool CAbstractThreadWorker::isAborted()
{
    return m_abortFlag.loadAcquire();
}

void CAbstractThreadWorker::resetAbortFlag()
{
    m_abortFlag.storeRelease(false);
    m_abortedFinished.storeRelease(false);
}
