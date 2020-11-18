#include <QMutexLocker>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QPointer>
#include <QThread>
#include <QDebug>
#include "utils/genericfuncs.h"
#include "downloadwriter.h"
#include "global/structures.h"
#include "global/control.h"

extern "C" {
#include <zip.h>
}

namespace CDefaults {
const int zipThreadMicrosleep = 10000;
const auto zipTempFilePtr = "zipTempFilePtr";
}

QAtomicInteger<int> CDownloadWriter::m_workCount;
QAtomicInteger<int> CZipWriter::m_zipWorkerCount;
QAtomicInteger<bool> CZipWriter::m_abort;

QUuid CDownloadWriter::getAuxId() const
{
    return m_auxId;
}

void CDownloadWriter::handleError(const QString &message)
{
    qCritical() << message;
    Q_EMIT error(message);
    Q_EMIT finished();
}

CDownloadWriter::CDownloadWriter(QObject *parent, const QString &zipFile, const QString &fileName, qint64 offset,
                                 const QUuid &auxId)
    : CAbstractThreadWorker(parent),
      m_zipFile(zipFile),
      m_fileName(fileName),
      m_auxId(auxId),
      m_offset(offset)
{
}

CDownloadWriter::~CDownloadWriter()
{
    m_zipData.clear();

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
        return tr("Download writer ZIP (%1/%2)")
                .arg(zfi.fileName(),fi.fileName());
    }

    qint64 filePos = 0L;
    if (m_rawFile.isOpen())
        filePos = m_rawFile.pos();
    return tr("Download writer (at %1 in %2)")
            .arg(CGenericFuncs::formatFileSize(filePos),fi.fileName());
}

void CDownloadWriter::startMain()
{
    if (exitIfAborted()) return;

    m_zipData.clear();

    if (m_zipFile.isEmpty()) {
        m_rawFile.setFileName(m_fileName);
        if (!m_rawFile.open(QIODevice::ReadWrite)) {
            handleError(tr("Unable to write file %1").arg(m_fileName));
            return;
        }
        m_rawFile.seek(m_offset);
        if (m_offset<m_rawFile.size())
            m_rawFile.resize(m_offset);
    }
}

void CDownloadWriter::appendBytesToFile(const QByteArray &data)
{
    if (exitIfAborted()) return;

    if (m_zipFile.isEmpty()) {
        if (!m_rawFile.isOpen()) {
            handleError(tr("File not opened for write %1").arg(m_fileName));
            return;
        }

        m_workCount++;
        m_rawFile.write(data);
        addLoadedRequest(data.size());
        m_workCount--;

    } else {
        if (m_zipData.isNull()) {
            QFileInfo fi(m_zipFile);
            m_zipData.reset(new QTemporaryFile(QSL("%1/").arg(fi.path())));
            if (!(m_zipData->open())) {
                handleError(tr("Unable to create temporary file for %1").arg(m_zipFile));
                return;
            }
        }
        m_workCount++;
        m_zipData->write(data);
        addLoadedRequest(data.size());
        m_workCount--;
    }
}

void CDownloadWriter::finalizeFile(bool success)
{
    if (exitIfAborted()) return;

    m_workCount++;

    if (m_zipFile.isEmpty()) {
        if (m_rawFile.isOpen()) {
            m_rawFile.flush();
            m_rawFile.close();
        }
    } else {
        if (success && !exitIfAborted()) {
            gSet->zipWriter()->appendFileToZip(m_fileName,m_zipFile,m_zipData);
        }
    }
    m_zipData.clear();

    m_workCount--;
    Q_EMIT writeComplete(success);
    Q_EMIT finished();
}

void CZipWriter::appendFileToZip(const QString &fileName, const QString &zipFile,
                                 const QSharedPointer<QTemporaryFile> &data)
{
    if (CZipWriter::m_abort.loadAcquire()) return;
    if (gSet == nullptr) return;
    if (gSet->zipWriter() == nullptr) return;

    QMutexLocker locker(&m_dataMutex);

    CZipWriterFileItem item;
    item.fileName = fileName;
    item.data = data;
    m_data[zipFile].files.append(item);

    QMetaObject::invokeMethod(gSet->zipWriter(),&CZipWriter::zipProcessing);
}

bool CZipWriter::isBusy()
{
    return (CZipWriter::m_zipWorkerCount.loadAcquire()>0);
}

void CZipWriter::abortAllWorkers()
{
    CZipWriter::m_abort.storeRelease(true);
}

void CZipWriter::terminateAllWorkers()
{
    Q_EMIT terminateWorkers();
}

CZipWriter::CZipWriter(QObject *parent)
    : QObject(parent)
{
}

CZipWriter::~CZipWriter() = default;

void CZipWriter::zipProcessing()
{
    QMutexLocker locker(&m_dataMutex);

    for (auto it = m_data.keyValueBegin(), end = m_data.keyValueEnd(); it != end; ++it) {
        if (!(*it).second.files.isEmpty() && (*it).second.thread.isNull()) {

            const QString zipFile = (*it).first;
            QThread* th = QThread::create([this,zipFile](){
                m_zipWorkerCount++;

                for (;;) {
                    QThread::usleep(CDefaults::zipThreadMicrosleep);
                    if (m_abort.loadAcquire()) break;

                    m_dataMutex.lock();
                    if (!m_data.contains(zipFile) || m_data.value(zipFile).files.isEmpty()) {
                        m_dataMutex.unlock();
                        break;
                    }
                    const QVector<CZipWriterFileItem> items = m_data.value(zipFile).files;
                    m_data[zipFile].files.clear();
                    m_dataMutex.unlock();
                    writeZipData(zipFile,items);
                }

                m_zipWorkerCount--;
            });
            connect(th,&QThread::finished,th,&QThread::deleteLater);
            connect(this,&CZipWriter::terminateWorkers,th,&QThread::terminate);

            th->setObjectName(QSL("ZIP_packer"));
            th->start();
            (*it).second.thread = th;
        }
    }
}

void CZipWriter::writeZipData(const QString& zipFile, const QVector<CZipWriterFileItem> &items)
{
    if (m_abort.loadAcquire()) return;

    int errorp = 0;
    zip_t* zip = zip_open(zipFile.toUtf8().constData(),ZIP_CREATE,&errorp);
    if (zip == nullptr) {
        zip_error_t ziperror;
        zip_error_init_with_code(&ziperror, errorp);
        const QString error = QSL("Unable to open zip file: %1 %2")
                              .arg(zipFile,QString::fromUtf8(zip_error_strerror(&ziperror)));
        Q_EMIT zipError(error);
        return;
    }

    QString error;
    for (const auto& file : items) {
        if (m_abort.loadAcquire()) break;
        Q_ASSERT(!file.fileName.isEmpty());
        Q_ASSERT(file.data->isOpen());

        qint64 size = file.data->size();
        uchar* ptr = file.data->map(0,size);
        Q_ASSERT(ptr != nullptr);
        file.data->setProperty(CDefaults::zipTempFilePtr,reinterpret_cast<qulonglong>(ptr));

        zip_source_t* src = nullptr;
        if ((src = zip_source_buffer(zip, ptr, static_cast<uint64_t>(size), 0)) == nullptr ||
                zip_file_add(zip, file.fileName.toUtf8().constData(), src, ZIP_FL_ENC_UTF_8) < 0) {
            zip_source_free(src);
            error = QSL("Error adding file to zip: %1 %2 %3")
                    .arg(zipFile,file.fileName,QString::fromUtf8(zip_strerror(zip)));
            break;
        }
    }

    if (m_abort.loadAcquire()) {
        zip_discard(zip);
    } else {
        zip_close(zip);
    }

    for (const auto& file : items) {
        uchar* ptr = reinterpret_cast<uchar *>(file.data->property(CDefaults::zipTempFilePtr).toULongLong());
        file.data->unmap(ptr);
        file.data->close();
    }

    if (!error.isEmpty()) {
        Q_EMIT zipError(error);
    }
}
