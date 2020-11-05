#ifndef THREADWORKER_H
#define THREADWORKER_H

#include <QObject>

class CAbstractTranslator;

class CAbstractThreadWorker : public QObject
{
    friend class CAbstractTranslator;

    Q_OBJECT
private:
    QAtomicInteger<bool> m_abortFlag;
    QAtomicInteger<bool> m_abortedFinished;
    qint64 m_loadedTotalSize { 0L };
    qint64 m_loadedRequestCount { 0L };

protected:
    bool exitIfAborted();
    bool isAborted();
    void resetAbortFlag();
    void addLoadedRequest(qint64 size);
    virtual void startMain() = 0;

public:
    explicit CAbstractThreadWorker(QObject *parent = nullptr);

public Q_SLOTS:
    void start();
    void abort();

Q_SIGNALS:
    void started();
    void finished();
    void dataLoaded();

};

#endif // THREADWORKER_H
