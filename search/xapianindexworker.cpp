#include <algorithm>
#include <execution>
#include <filesystem>

#include "xapianindexworker_p.h"
#include "xapianindexworker.h"

#include <QMutex>
#include <QMutexLocker>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QScopeGuard>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}

#include "global/structures.h"
#include "global/control.h"
#include "utils/genericfuncs.h"
#include "utils/htmlparser.h"

#include <QDebug>

namespace CDefaults {
const int progressMsgFrequency = 1000;
const auto docIDPrefix = "QFH";
}

CXapianIndexWorker::CXapianIndexWorker(QObject *parent, bool cleanupDatabase)
    : CAbstractThreadWorker(parent),
      dptr(new CXapianIndexWorkerPrivate(this))
{
    Q_D(CXapianIndexWorker);

    d->m_cleanupDatabase = cleanupDatabase;
    d->m_stemLang = gSet->settings()->xapianStemmerLang.toStdString();
    d->m_indexDirs = gSet->settings()->xapianIndexDirList;
    d->m_cacheDir.setPath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
}

CXapianIndexWorker::~CXapianIndexWorker() = default;

void CXapianIndexWorker::forceScanDirList(const QStringList &forcedScanDirList)
{
    Q_D(CXapianIndexWorker);

    d->m_indexDirs = forcedScanDirList;
    d->m_cleanupDatabase = false;
    d->m_fastIncrementalIndex = true;
}

void CXapianIndexWorker::startMain()
{
    Q_D(CXapianIndexWorker);

#ifdef WITH_XAPIAN
    static QMutex mainIndexerMutex;
    QMutexLocker locker(&mainIndexerMutex);

    if (!d->m_cacheDir.isReadable()) return;
    const QString dbFile = d->m_cacheDir.filePath(QSL("xapian_index"));

    QString errMsg;
    QElapsedTimer tmr;
    tmr.start();

    int dbMode = Xapian::DB_CREATE_OR_OPEN;
    if (d->m_cleanupDatabase)
        dbMode = Xapian::DB_CREATE_OR_OVERWRITE;

    try {
        d->m_db.reset(new Xapian::WritableDatabase(dbFile.toStdString(), dbMode));
    } catch (const Xapian::Error &err) {
        errMsg = QString::fromStdString(err.get_msg());
    } catch (const std::string &s) {
        errMsg = QString::fromStdString(s);
    } catch (const char *s) {
        errMsg = QString::fromUtf8(s);
    }
    if (!errMsg.isEmpty()) {
        errMsg = QSL("XapianIndexer: Xapian database creation exception: %1").arg(errMsg);
        Q_EMIT errorOccured(errMsg);
        qCritical() << errMsg;
        return;
    }

    auto dbCleanup = qScopeGuard([this,d]{
        QString errMsgSG;
        try {
            d->m_db->commit();
            d->m_db->close();
        } catch (const Xapian::Error &err) {
            errMsgSG = QString::fromStdString(err.get_msg());
        } catch (const std::string &s) {
            errMsgSG = QString::fromStdString(s);
        } catch (const char *s) {
            errMsgSG = QString::fromUtf8(s);
        }
        if (!errMsgSG.isEmpty()) {
            errMsgSG = QSL("XapianIndexer: Xapian database sync exception: %1").arg(errMsgSG);
            Q_EMIT errorOccured(errMsgSG);
            qCritical() << errMsgSG;
        }
        Q_EMIT finished();
    });

    std::map<std::string,QString> fsFiles;
    for (const auto &dir : qAsConst(d->m_indexDirs)) {
        QDirIterator it(dir, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString fi = it.next();
            std::string docID;
            QString fsuffix;
            qint64 fsize = 0;
            if (fileMeta(fi,docID,fsize,fsuffix))
                fsFiles.emplace(docID,fi);
            if (isAborted()) {
                qWarning() << QSL("XapianIndexer: Aborting.");
                return;
            }
        }
    }
    qInfo() << QSL("XapianIndexer: Filesystem scan: %1 files (%2 ms).").arg(fsFiles.size()).arg(tmr.elapsed());

    tmr.restart();
    std::map<std::string,QString> baseIDs;
    const std::string prefix(CDefaults::docIDPrefix);
    try {
        for (auto it = d->m_db->allterms_begin(prefix), end = d->m_db->allterms_end(prefix); it != end; ++it) {
            baseIDs.emplace(*it,QString());
            if (isAborted()) {
                qWarning() << QSL("XapianIndexer: Aborting.");
                return;
            }
        }
    } catch (const Xapian::Error &err) {
        errMsg = QString::fromStdString(err.get_msg());
    } catch (const std::string &s) {
        errMsg = QString::fromStdString(s);
    } catch (const char *s) {
        errMsg = QString::fromUtf8(s);
    }
    if (!errMsg.isEmpty()) {
        errMsg = QSL("XapianIndexer: Xapian database initial scan exception: %1").arg(errMsg);
        Q_EMIT errorOccured(errMsg);
        qCritical() << errMsg;
        return;
    }
    qInfo() << QSL("XapianIndexer: Base contents: %1 files (%2 ms).").arg(baseIDs.size()).arg(tmr.elapsed());

    tmr.restart();
    std::vector<std::pair<std::string,QString> > newFiles;
    std::set_difference(fsFiles.cbegin(),fsFiles.cend(),baseIDs.cbegin(),baseIDs.cend(),
                        std::back_inserter(newFiles),
                        [](const auto& s1, const auto& s2){
        return s1.first < s2.first;
    });
    qInfo() << QSL("XapianIndexer: Files to add: %1 (%2 ms).").arg(newFiles.size()).arg(tmr.elapsed());

    if (!d->m_fastIncrementalIndex) {
        tmr.restart();
        std::vector<std::pair<std::string,QString> > deletedFiles;
        std::set_difference(baseIDs.cbegin(),baseIDs.cend(),fsFiles.cbegin(),fsFiles.cend(),
                            std::back_inserter(deletedFiles),
                            [](const auto& s1, const auto& s2){
            return s1.first < s2.first;
        });
        qInfo() << QSL("XapianIndexer: Files to remove from database: %1 (%2 ms).").arg(deletedFiles.size()).arg(tmr.elapsed());

        tmr.restart();
        try {
            if (!deletedFiles.empty()) {
                for (const auto& fpair : deletedFiles) {
                    d->m_db->delete_document(fpair.first);
                    if (isAborted()) {
                        qWarning() << QSL("XapianIndexer: Aborting.");
                        return;
                    }
                }
                qInfo() << QSL("XapianIndexer: Deleted files was removed from base (%1 ms).").arg(tmr.elapsed());
            }
        } catch (const Xapian::Error &err) {
            errMsg = QString::fromStdString(err.get_msg());
        } catch (const std::string &s) {
            errMsg = QString::fromStdString(s);
        } catch (const char *s) {
            errMsg = QString::fromUtf8(s);
        }
        if (!errMsg.isEmpty()) {
            errMsg = QSL("XapianIndexer: Xapian database deleted cleanup exception: %1").arg(errMsg);
            Q_EMIT errorOccured(errMsg);
            qCritical() << errMsg;
            return;
        }
    }

    tmr.restart();
    if (!newFiles.empty()) {
        QAtomicInteger<int> cnt(0);
        qInfo() << QSL("XapianIndexer: Indexing %1 files...").arg(newFiles.size());
        std::for_each(std::execution::par, newFiles.begin(), newFiles.end(),
                      [this,&cnt](const auto& fpair) {
            if (handleFile(fpair.second))
                cnt++;
            if (cnt % CDefaults::progressMsgFrequency == 0) {
                const QString msg = tr("Xapian: Indexed %1 files...").arg(cnt);
                qInfo() << msg;
                Q_EMIT statusMessage(msg);
            }
            if (isAborted()) {
                qWarning() << QSL("XapianIndexer: Aborting.");
                return;
            }
        });
        qInfo() << QSL("XapianIndexer: Indexed %1 files to database (%2 ms).").arg(cnt).arg(tmr.elapsed());
    }
    qInfo() << QSL("XapianIndexer: Scan finished.");
#endif
}

QString CXapianIndexWorker::workerDescription() const
{
    const Q_D(CXapianIndexWorker);

#ifdef WITH_XAPIAN
    if (d->m_db.isNull())
        return tr("Xapian indexer (initializing)");

    return tr("Xapian indexer (%1 root dirs)").arg(d->m_indexDirs.count());
#else
    return QString();
#endif
}

bool CXapianIndexWorker::fileMeta(const QString& filename, std::string &docID, qint64 &size, QString &suffix)
{
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    const QByteArray cfile = filename.toUtf8();
    sha1.addData(cfile);

    struct stat attrib {};
    // QFileInfo too slow
    if (stat(cfile.constData(),&attrib)!=0)
        return false;

    const auto mtime = attrib.st_mtime;
    sha1.addData(reinterpret_cast<const char *>(&mtime),sizeof(mtime));
    const auto ctime = attrib.st_ctime;
    sha1.addData(reinterpret_cast<const char *>(&ctime),sizeof(ctime));
    const auto fsize = attrib.st_size;
    sha1.addData(reinterpret_cast<const char *>(&fsize),sizeof(fsize));

    docID = CDefaults::docIDPrefix;
    docID.append(sha1.result().toHex().constData());

    size = fsize;
    suffix = QString::fromStdString(std::filesystem::path(cfile.constData()).extension().string());
    return true;
}

bool CXapianIndexWorker::handleFile(const QString &filename)
{
    const Q_D(CXapianIndexWorker);

#ifdef WITH_XAPIAN
    QString mime;
    QByteArray textContent;

    std::string docID;
    qint64 fileSize = 0;
    QString fileSuffix;
    if (!fileMeta(filename,docID,fileSize,fileSuffix))
        return false;

    // ****** Read file
    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
        textContent = f.readAll();
        f.close();

        // ****** Parse file
        mime = CGenericFuncs::detectMIME(textContent);
        if (mime.startsWith(QSL("text/html"),Qt::CaseInsensitive)) // HTML file
        {
            CHTMLNode doc(CHTMLParser::parseHTML(CGenericFuncs::detectDecodeToUnicode(textContent)));
            QString html;
            CHTMLParser::generatePlainText(doc,html);
            textContent = html.toUtf8();
        } else if (fileSuffix.toLower() != QSL(".txt")) { // and TXT files are supported
            // skip other files
            textContent.clear();
        }
    } else {
        qWarning() << QSL("XapianIndexer: Unable to open file %1").arg(filename);
        textContent.clear();
    }

    Xapian::Document doc;
    QString errMsg;

    try {
        if (!textContent.isEmpty()) {
            const QByteArray utf8Content = CGenericFuncs::detectDecodeToUtf8(textContent);
            if (!utf8Content.isEmpty()) {
                // ******* Add file to database
                Xapian::TermGenerator generator;
                if (!d->m_stemLang.empty())
                    generator.set_stemmer(Xapian::Stem(d->m_stemLang));
                generator.set_flags(Xapian::TermGenerator::FLAG_CJK_WORDS);
                generator.set_document(doc);
                generator.index_text(utf8Content.toStdString());
            } else {
                qCritical() << QSL("XapianIndexer: Unable to decode file %1 to UTF-8").arg(filename);
                textContent.clear(); // codepage error
            }
        }
    } catch (const Xapian::Error &err) {
        errMsg = QString::fromStdString(err.get_msg());
    } catch (const std::string &s) {
        errMsg = QString::fromStdString(s);
    } catch (const char *s) {
        errMsg = QString::fromUtf8(s);
    }
    if (!errMsg.isEmpty()) {
        qCritical() << QSL("XapianIndexer: Xapian term generator exception: %1").arg(errMsg);
        textContent.clear(); // xapian error
    }

    // Write empty document for failed or unknown file to avoid rescan.

    try {
        doc.set_data(filename.toStdString());
        doc.add_boolean_term(docID);
        static QMutex dbMutex;
        {
            QMutexLocker locker(&dbMutex);
            d->m_db->replace_document(docID,doc);
        }
    } catch (const Xapian::Error &err) {
        errMsg = QString::fromStdString(err.get_msg());
    } catch (const std::string &s) {
        errMsg = QString::fromStdString(s);
    } catch (const char *s) {
        errMsg = QString::fromUtf8(s);
    }
    if (!errMsg.isEmpty()) {
        qCritical() << QSL("XapianIndexer: Xapian database writing exception: %1").arg(errMsg);
        textContent.clear();
    }

    if (!textContent.isEmpty())
        addLoadedRequest(textContent.size());

#endif
    return true;
}

CXapianIndexWorkerPrivate::CXapianIndexWorkerPrivate(QObject *parent)
    : QObject(parent)
{
}
