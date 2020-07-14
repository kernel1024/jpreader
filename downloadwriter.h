#ifndef DOWNLOADWRITER_H
#define DOWNLOADWRITER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QAtomicInteger>
#include <QUuid>

class CDownloadWriter : public QObject
{
    Q_OBJECT
private:
    QAtomicInteger<bool> m_terminate;
    QAtomicInteger<int> m_workCount;
    void handleError(const QString& message, const QUuid &uuid);

public:
    explicit CDownloadWriter(QObject *parent = nullptr);

public Q_SLOTS:
    void writeBytesToZip(const QString &zipFile, const QString &fileName,
                         const QByteArray &data, const QUuid& uuid);
    void writeBytesToFile(const QString &fileName, const QByteArray &data,
                          const QUuid& uuid);
    void terminateStop();

Q_SIGNALS:
    void finished();
    void error(const QString& message, const QUuid& uuid);
    void writeComplete(const QUuid& uuid);

};

#endif // DOWNLOADWRITER_H
