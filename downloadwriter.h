#ifndef DOWNLOADWRITER_H
#define DOWNLOADWRITER_H

#include <QObject>
#include <QString>
#include <QAtomicInteger>
#include <QMutex>
#include <QFile>
#include <QUuid>
#include <QPointer>
#include <QTemporaryFile>
#include <QTimer>
#include "abstractthreadworker.h"

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
    void finalizeFile(bool success);

Q_SIGNALS:
    void error(const QString& message);
    void writeComplete(bool success);

};

class CZipWriterItem
{
public:
    QString fileName;
    QString zipFile;
    QSharedPointer<QTemporaryFile> data;

    CZipWriterItem() = default;
    CZipWriterItem(const CZipWriterItem& other) = default;
    CZipWriterItem(const QString& aFileName, const QString& aZipFile);
    ~CZipWriterItem() = default;
    CZipWriterItem &operator=(const CZipWriterItem& other) = default;
    bool operator==(const CZipWriterItem &s) const;
    bool operator!=(const CZipWriterItem &s) const;
    bool isEmpty() const;
};

class CZipWriter : public QObject
{
    Q_OBJECT
private:
    QMutex m_dataMutex;
    QMutex m_threadMutex;
    QPointer<QThread> m_zipThread;
    QVector<CZipWriterItem> m_data;
    QTimer m_wakeupTimer;

    Q_DISABLE_COPY(CZipWriter)

    void zipProcessing();
    void writeZipData(const CZipWriterItem& item);

public:
    explicit CZipWriter(QObject *parent = nullptr);
    ~CZipWriter() override;

    static void appendFileToZip(const QString& fileName, const QString& zipFile, const QSharedPointer<QTemporaryFile> &data);
    static bool isBusy();

Q_SIGNALS:
    void zipError(const QString& message);

};

#endif // DOWNLOADWRITER_H
