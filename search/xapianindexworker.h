#ifndef XAPIANINDEXWORKER_H
#define XAPIANINDEXWORKER_H

#include <string>

#ifdef WITH_XAPIAN
#include <xapian.h>
#endif

#include <QObject>
#include <QDir>
#include <QScopedPointer>
#include "abstractthreadworker.h"

class CXapianIndexWorker : public CAbstractThreadWorker
{
    Q_OBJECT
private:
    Q_DISABLE_COPY(CXapianIndexWorker)
#ifdef WITH_XAPIAN
    QScopedPointer<Xapian::WritableDatabase> m_db;
#endif
    bool m_cleanDB { false };
    std::string m_stemLang;
    QStringList m_indexDirs;
    QDir m_cacheDir;

    bool fileMeta(const QString& filename, std::string &docID, qint64 &size, QString &suffix);
    bool handleFile(const QString& filename);

public:
    enum WorkerMode { xwIndexFull, xwClearAndIndexFull };
    Q_ENUM(WorkerMode)

    CXapianIndexWorker(QObject *parent = nullptr, const QStringList &forcedScanDirList = QStringList(),
                       bool cleanDB = false);
    ~CXapianIndexWorker() override = default;

    static void startupXapianWorkerEx();

protected:
    void startMain() override;
    QString workerDescription() const override;

};

#endif // XAPIANINDEXWORKER_H
