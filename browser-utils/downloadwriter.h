#ifndef DOWNLOADWRITER_H
#define DOWNLOADWRITER_H

#include <QObject>
#include <QHash>
#include <QVector>
#include <QString>
#include <QAtomicInteger>
#include <QMutex>
#include <QFile>
#include <QUuid>
#include <QPointer>
#include <QTemporaryFile>
#include <QTimer>
#include "abstractthreadworker.h"

namespace CDefaults {
const int maxZipWriterErrorsInteractive = 5;
}

class CDownloadWriter : public CAbstractThreadWorker
{
    Q_OBJECT
private:
    QSharedPointer<QTemporaryFile> m_zipData;
    QString m_zipFile;
    QString m_fileName;
    QFile m_rawFile;
    QUuid m_auxId;
    qint64 m_offset { 0L };

    static QAtomicInteger<int> m_workCount;

    void handleError(const QString& message);

public:
    CDownloadWriter(QObject *parent,
                    const QString &zipFile, const QString &fileName,
                    qint64 offset, const QUuid &auxId);
    ~CDownloadWriter() override;
    static int getWorkCount();
    QString workerDescription() const override;
    QUuid getAuxId() const;

protected:
    void startMain() override;

public Q_SLOTS:
    void appendBytesToFile(const QByteArray &data);
    void finalizeFile(bool success, bool forceDelete);

Q_SIGNALS:
    void error(const QString& message);
    void writeComplete(bool success);

};

struct CZipWriterFileItem
{
    QString fileName;
    QSharedPointer<QTemporaryFile> data;
};

struct CZipWriterWorkerItem
{
    QPointer<QThread> thread;
    QVector<CZipWriterFileItem> files;
};

class CZipWriter : public QObject
{
    Q_OBJECT
private:
    QMutex m_dataMutex;
    QHash<QString, CZipWriterWorkerItem> m_data;
    static QAtomicInteger<int> m_zipWorkerCount;
    static QAtomicInteger<bool> m_abort;

    Q_DISABLE_COPY(CZipWriter)

    void zipProcessing();
    void writeZipData(const QString &zipFile, const QVector<CZipWriterFileItem>& items);

public:
    explicit CZipWriter(QObject *parent = nullptr);
    ~CZipWriter() override;

    void appendFileToZip(const QString& fileName, const QString& zipFile, const QSharedPointer<QTemporaryFile> &data);
    static bool isBusy();
    void abortAllWorkers();
    void terminateAllWorkers();

Q_SIGNALS:
    void zipError(const QString& message);
    void terminateWorkers();

};

#endif // DOWNLOADWRITER_H
