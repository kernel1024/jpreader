#include <QMutexLocker>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QPointer>
#include <QThread>
#include <QDebug>
#include "genericfuncs.h"
#include "downloadwriter.h"
#include "structures.h"
#include "globalcontrol.h"

extern "C" {
#include <zip.h>
}

namespace CDefaults {
const int zipThreadMicrosleep = 10000;
}

QAtomicInteger<int> CDownloadWriter::m_workCount;

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
        qint64 dataSize = 0L;
        if (m_zipData)
            dataSize = m_zipData->size();
        return tr("Download writer ZIP (%1 to %2/%3)")
                .arg(CGenericFuncs::formatFileSize(dataSize),
                     zfi.fileName(),fi.fileName());
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
    if (m_zipFile.isEmpty()) {
        if (!m_rawFile.isOpen()) {
            handleError(tr("File not opened for write %1").arg(m_fileName));
            return;
        }

        m_workCount++;
        m_rawFile.write(data);
        m_workCount--;

    } else {
        if (m_zipData.isNull()) {
            QFileInfo fi(m_zipFile);
            m_zipData.reset(new QTemporaryFile(fi.path()));
            if (!(m_zipData->open())) {
                handleError(tr("Unable to create temporary file for %1").arg(m_zipFile));
                return;
            }
            const QString fnm = m_zipData->fileName();
            qDebug() << fnm;
            connect(m_zipData.data(),&QTemporaryFile::destroyed,[fnm](){
                qDebug() << "tempfile deleted!" << fnm;
            });
        }
        m_zipData->write(data);
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
        if (success && !exitIfAborted()) {
            m_zipData->close();
            CZipWriter::appendFileToZip(m_fileName,m_zipFile,m_zipData);
        }
    }
    m_zipData.clear();

    m_workCount--;
    Q_EMIT writeComplete(success);
    Q_EMIT finished();
}

CZipWriterItem::CZipWriterItem(const QString &aFileName, const QString &aZipFile)
{
    fileName = aFileName;
    zipFile = aZipFile;
}

bool CZipWriterItem::operator==(const CZipWriterItem &s) const
{
    return ((fileName == s.fileName) && (zipFile == s.zipFile));
}

bool CZipWriterItem::operator!=(const CZipWriterItem &s) const
{
    return !operator==(s);
}

bool CZipWriterItem::isEmpty() const
{
    return (zipFile.isEmpty() && fileName.isEmpty());
}

void CZipWriter::appendFileToZip(const QString &fileName, const QString &zipFile,
                                 const QSharedPointer<QTemporaryFile> &data)
{
    if (gSet == nullptr) return;
    if (gSet->zipWriter() == nullptr) return;

    QMutexLocker locker(&(gSet->zipWriter()->m_dataMutex));

    CZipWriterItem item(fileName,zipFile);
    item.data = data;
    gSet->zipWriter()->m_data.append(item);

    QMetaObject::invokeMethod(gSet->zipWriter(),&CZipWriter::zipProcessing);
}

bool CZipWriter::isBusy()
{
    if ((gSet == nullptr) || (gSet->zipWriter() == nullptr)) return false;
    return (!(gSet->zipWriter()->m_zipThread.isNull()));
}

CZipWriter::CZipWriter(QObject *parent)
    : QObject(parent)
{
}

CZipWriter::~CZipWriter() = default;

void CZipWriter::zipProcessing()
{
    QMutexLocker locker(&m_threadMutex);
    if (m_zipThread) return;

    QThread* th = QThread::create([this](){

        for (;;) {
            QThread::usleep(CDefaults::zipThreadMicrosleep);
            CZipWriterItem item;
            {
                QMutexLocker dataLock(&m_dataMutex);
                if (m_data.isEmpty())
                    break;
                item = m_data.takeFirst();
            }
            writeZipData(item);
        }
    });
    connect(th,&QThread::finished,th,&QThread::deleteLater);
    th->start();
    m_zipThread = th;
}

void CZipWriter::writeZipData(const CZipWriterItem &item)
{
    Q_ASSERT(!item.isEmpty());

    int errorp = 0;
    zip_t* zip = zip_open(item.zipFile.toUtf8().constData(),ZIP_CREATE,&errorp);
    if (zip == nullptr) {
        zip_error_t ziperror;
        zip_error_init_with_code(&ziperror, errorp);
        const QString error = QSL("Unable to open zip file: %1 %2 %3")
                              .arg(item.zipFile,item.fileName,QString::fromUtf8(zip_error_strerror(&ziperror)));
        Q_EMIT zipError(error);
        return;
    }

    if (!item.data->open()) {
        const QString error = QSL("Unable to open temporary file: %1 %2")
                              .arg(item.zipFile,item.fileName);
        Q_EMIT zipError(error);
        return;
    }
    qint64 size = item.data->size();
    uchar* ptr = item.data->map(0,size);
    Q_ASSERT(ptr != nullptr);

    zip_source_t* src = nullptr;
    QString error;
    if ((src = zip_source_buffer(zip, ptr, static_cast<uint64_t>(size), 0)) == nullptr ||
            zip_file_add(zip, item.fileName.toUtf8().constData(), src, ZIP_FL_ENC_UTF_8) < 0) {
        zip_source_free(src);
        error = QSL("Error adding file to zip: %1 %2 %3")
                              .arg(item.zipFile,item.fileName,QString::fromUtf8(zip_strerror(zip)));
    }
    zip_close(zip);
    item.data->unmap(ptr);
    item.data->close();

    if (!error.isEmpty()) {
        Q_EMIT zipError(error);
    }
}
