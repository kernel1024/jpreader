#include <QMutexLocker>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QPointer>
#include <QDebug>
#include "genericfuncs.h"
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

CDownloadWriter::CDownloadWriter(QObject *parent, const QString &zipFile, const QString &fileName, qint64 offset, const QUuid &uuid)
    : CAbstractThreadWorker(parent),
      m_zipFile(zipFile),
      m_fileName(fileName),
      m_uuid(uuid),
      m_offset(offset)
{
}

CDownloadWriter::~CDownloadWriter()
{
    m_zipAccumulator.clear();

    if (m_rawFile.isOpen()) {
        m_rawFile.flush();
        m_rawFile.close();
    }
}

int CDownloadWriter::getWorkCount()
{
    return m_workCount.loadAcquire();
}

QString CDownloadWriter::workerDescription() const
{
    QFileInfo fi(m_fileName);
    if (!m_zipFile.isEmpty()) {
        QFileInfo zfi(m_zipFile);
        return tr("Download writer ZIP (%1 to %2/%3)")
                .arg(CGenericFuncs::formatFileSize(m_zipAccumulator.size()),
                     zfi.fileName(),fi.fileName());
    }

    qint64 filePos = 0L;
    if (m_rawFile.isOpen())
        filePos = m_rawFile.pos();
    return tr("Download writer (at %1 in %2)")
            .arg(CGenericFuncs::formatFileSize(filePos),fi.fileName());
}

void CDownloadWriter::writeBytesToZip()
{
    if (m_zipAccumulator.empty()) return;

    if (exitIfAborted()) {
        m_zipAccumulator.clear();
        return;
    }

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
    if ((src = zip_source_buffer(zip, m_zipAccumulator.data(),
                                 static_cast<uint64_t>(m_zipAccumulator.size()), 0)) == nullptr ||
            zip_file_add(zip, m_fileName.toUtf8().constData(), src, ZIP_FL_ENC_UTF_8) < 0) {
        zip_source_free(src);
        zip_close(zip);
        const QString error = QSL("Error adding file to zip: %1 %2 %3")
                              .arg(m_zipFile,m_fileName,QString::fromUtf8(zip_strerror(zip)));
        handleError(error,m_uuid);
        return;
    }
    m_zipAccumulator.clear();
    zip_close(zip);
}

void CDownloadWriter::startMain()
{
    if (exitIfAborted()) return;

    m_zipAccumulator.clear();

    if (m_zipFile.isEmpty()) {
        m_rawFile.setFileName(m_fileName);
        if (!m_rawFile.open(QIODevice::ReadWrite)) {
            handleError(tr("Unable to write file %1").arg(m_fileName),m_uuid);
            return;
        }
        m_rawFile.seek(m_offset);
        if (m_offset<m_rawFile.size())
            m_rawFile.resize(m_offset);
    }
}

void CDownloadWriter::appendBytesToFile(const QByteArray &data)
{
    if (m_zipFile.isEmpty()) {
        if (!m_rawFile.isOpen()) {
            handleError(tr("File not opened for write %1").arg(m_fileName),m_uuid);
            return;
        }

        m_workCount++;
        m_rawFile.write(data);
        m_workCount--;

    } else {
        m_zipAccumulator.insert(m_zipAccumulator.end(), data.constData(),
                                data.constData() + data.size()); // NOLINT
    }
}

void CDownloadWriter::finalizeFile(bool success)
{
    m_workCount++;

    if (m_zipFile.isEmpty()) {
        if (m_rawFile.isOpen()) {
            m_rawFile.flush();
            m_rawFile.close();
        }
    } else {
        if (success) {
            writeBytesToZip();
        } else {
            m_zipAccumulator.clear();
        }
    }

    m_workCount--;
    Q_EMIT writeComplete(m_uuid);
    Q_EMIT finished();
}
