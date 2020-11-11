#ifndef DOWNLOADWRITER_H
#define DOWNLOADWRITER_H

#include <QObject>
#include <QString>
#include <vector>
#include <QAtomicInteger>
#include <QUuid>
#include <QMutex>
#include <QFile>
#include "abstractthreadworker.h"

class CDownloadWriter : public CAbstractThreadWorker
{
    Q_OBJECT
private:
    std::vector<char> m_zipAccumulator;
    QString m_zipFile;
    QString m_fileName;
    QUuid m_uuid;
    QFile m_rawFile;
    qint64 m_offset { 0L };

    static QAtomicInteger<int> m_workCount;
    static QMutex zipLock;

    void handleError(const QString& message, const QUuid &uuid);
    void writeBytesToZip();

public:
    CDownloadWriter(QObject *parent,
                    const QString &zipFile, const QString &fileName,
                    qint64 offset, const QUuid &uuid);
    ~CDownloadWriter() override;
    static int getWorkCount();
    QString workerDescription() const override;

protected:
    void startMain() override;

public Q_SLOTS:
    void appendBytesToFile(const QByteArray &data);
    void finalizeFile(bool success);

Q_SIGNALS:
    void error(const QString& message, const QUuid& uuid);
    void writeComplete(const QUuid& uuid);

};

#endif // DOWNLOADWRITER_H
