#include <QMutexLocker>
#include <QFile>
#include <QDebug>
#include "downloadwriter.h"
#include "structures.h"

extern "C" {
#include <zip.h>
}

void CDownloadWriter::handleError(const QString &message, const QUuid& uuid)
{
    qCritical() << message;
    Q_EMIT error(message, uuid);
}

CDownloadWriter::CDownloadWriter(QObject *parent) : QObject(parent)
{
}

void CDownloadWriter::writeBytesToZip(const QString &zipFile, const QString &fileName,
                                      const QByteArray &data, const QUuid &uuid)
{
    if (m_terminate) return;

    static QMutex zipLock;
    QMutexLocker locker(&zipLock);
    m_workCount++;

    int errorp = 0;
    zip_t* zip = zip_open(zipFile.toUtf8().constData(),ZIP_CREATE,&errorp);
    if (zip == nullptr) {
        zip_error_t ziperror;
        zip_error_init_with_code(&ziperror, errorp);
        const QString error = QSL("Unable to open zip file: %1 %2 %3")
                              .arg(zipFile,fileName,QString::fromUtf8(zip_error_strerror(&ziperror)));
        handleError(error,uuid);
        return;
    }

    zip_source_t* src = nullptr;
    if ((src = zip_source_buffer(zip, data.constData(), static_cast<uint64_t>(data.size()), 0)) == nullptr ||
            zip_file_add(zip, fileName.toUtf8().constData(), src, ZIP_FL_ENC_UTF_8) < 0) {
        zip_source_free(src);
        zip_close(zip);
        const QString error = QSL("Error adding file to zip: %1 %2 %3")
                              .arg(zipFile,fileName,QString::fromUtf8(zip_strerror(zip)));
        handleError(error,uuid);
        return;
    }

    zip_close(zip);

    Q_EMIT writeComplete(uuid);

    m_workCount--;
    if (m_workCount<1 && m_terminate) {
        Q_EMIT finished();
    }
}

void CDownloadWriter::writeBytesToFile(const QString &fileName, const QByteArray &data,
                                       const QUuid &uuid)
{
    if (m_terminate.loadAcquire()) return;
    m_workCount++;

    QFile f(fileName); // TODO: continuous file download for big files
    if (f.open(QIODevice::WriteOnly)) {
        f.write(data);
        f.close();
    } else {
        handleError(tr("Unable to write file %1").arg(fileName),uuid);
    }

    Q_EMIT writeComplete(uuid);

    m_workCount--;
    if (m_workCount<1 && m_terminate) {
        Q_EMIT finished();
    }
}

void CDownloadWriter::terminateStop()
{
    m_terminate = true;
    if (m_workCount < 1) {
        Q_EMIT finished();
    }
}
