#include <QMutexLocker>
#include <QFile>
#include <QDebug>
#include "downloadwriter.h"
#include "structures.h"

extern "C" {
#include <zip.h>
}

QAtomicInteger<int> CDownloadWriter::m_workCount;
QMutex CDownloadWriter::zipLock;

void CDownloadWriter::handleError(const QString &message, const QUuid& uuid)
{
    qCritical() << message;
    Q_EMIT error(message, uuid);
}

CDownloadWriter::CDownloadWriter(QObject *parent, const QString &zipFile, const QString &fileName,
                                 const QByteArray &data, const QUuid &uuid)
    : CAbstractThreadWorker(parent),
      m_zipFile(zipFile),
      m_fileName(fileName),
      m_data(data),
      m_uuid(uuid)
{
}

int CDownloadWriter::getWorkCount()
{
    return m_workCount.loadAcquire();
}

void CDownloadWriter::writeBytesToZip()
{
    QMutexLocker locker(&zipLock);

    int errorp = 0;
    zip_t* zip = zip_open(m_zipFile.toUtf8().constData(),ZIP_CREATE,&errorp);
    if (zip == nullptr) {
        zip_error_t ziperror;
        zip_error_init_with_code(&ziperror, errorp);
        const QString error = QSL("Unable to open zip file: %1 %2 %3")
                              .arg(m_zipFile,m_fileName,QString::fromUtf8(zip_error_strerror(&ziperror)));
        handleError(error,m_uuid);
        return;
    }

    zip_source_t* src = nullptr;
    if ((src = zip_source_buffer(zip, m_data.constData(), static_cast<uint64_t>(m_data.size()), 0)) == nullptr ||
            zip_file_add(zip, m_fileName.toUtf8().constData(), src, ZIP_FL_ENC_UTF_8) < 0) {
        zip_source_free(src);
        zip_close(zip);
        const QString error = QSL("Error adding file to zip: %1 %2 %3")
                              .arg(m_zipFile,m_fileName,QString::fromUtf8(zip_strerror(zip)));
        handleError(error,m_uuid);
        return;
    }

    zip_close(zip);
}

void CDownloadWriter::writeBytesToFile()
{
    QFile f(m_fileName); // TODO: continuous file download for big files
    if (f.open(QIODevice::WriteOnly)) {
        f.write(m_data);
        f.close();
    } else {
        handleError(tr("Unable to write file %1").arg(m_fileName),m_uuid);
    }
}

void CDownloadWriter::startMain()
{
    if (exitIfAborted()) return;
    m_workCount++;

    if (m_zipFile.isEmpty()) {
        writeBytesToFile();
    } else {
        writeBytesToZip();
    }

    m_workCount--;
    Q_EMIT writeComplete(m_uuid);
    Q_EMIT finished();
}
