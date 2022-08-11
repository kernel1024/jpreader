#ifndef XAPIANINDEXWORKER_H
#define XAPIANINDEXWORKER_H

#include <string>
#include <QObject>
#include <QDir>
#include <QScopedPointer>
#include "abstractthreadworker.h"

class CXapianIndexWorkerPrivate;

class CXapianIndexWorker : public CAbstractThreadWorker
{
    Q_OBJECT
private:
    Q_DISABLE_COPY(CXapianIndexWorker)
    Q_DECLARE_PRIVATE_D(dptr,CXapianIndexWorker)

    QScopedPointer<CXapianIndexWorkerPrivate> dptr;

    bool fileMeta(const QString& filename, std::string &docID, qint64 &size, QString &suffix);
    bool handleFile(const QString& filename);

public:
    explicit CXapianIndexWorker(QObject *parent = nullptr, bool cleanupDatabase = false);
    ~CXapianIndexWorker() override;
    void forceScanDirList(const QStringList &forcedScanDirList);

    static void startupXapianWorkerEx();

protected:
    void startMain() override;
    QString workerDescription() const override;

};

#endif // XAPIANINDEXWORKER_H
