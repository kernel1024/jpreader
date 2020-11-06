#ifndef DOWNLOADWRITER_H
#define DOWNLOADWRITER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QAtomicInteger>
#include <QUuid>
#include <QMutex>
#include "abstractthreadworker.h"

class CDownloadWriter : public CAbstractThreadWorker
{
    Q_OBJECT
private:
    QString m_zipFile;
    QString m_fileName;
    QByteArray m_data;
    QUuid m_uuid;

    static QAtomicInteger<int> m_workCount;
    static QMutex zipLock;

    void handleError(const QString& message, const QUuid &uuid);
    void writeBytesToZip();
    void writeBytesToFile();

public:
    CDownloadWriter(QObject *parent,
                    const QString &zipFile, const QString &fileName,
                    const QByteArray &data, const QUuid &uuid);
    static int getWorkCount();
    QString workerDescription() const override;

protected:
    void startMain() override;

Q_SIGNALS:
    void error(const QString& message, const QUuid& uuid);
    void writeComplete(const QUuid& uuid);

};

#endif // DOWNLOADWRITER_H
