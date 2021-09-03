#include "xapianindexworker.h"

#include <algorithm>
#include <execution>
#include <filesystem>

#include <QMutex>
#include <QMutexLocker>
#include <QTextCodec>
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
const auto docIDPrefix = "QFH";
}

CXapianIndexWorker::CXapianIndexWorker(QObject *parent, const QStringList& forcedScanDirList, bool cleanDB)
    : CAbstractThreadWorker(parent),
      m_cleanDB(cleanDB)
{
    m_stemLang = gSet->settings()->xapianStemmerLang.toStdString();
    if (forcedScanDirList.isEmpty()) {
        m_indexDirs = gSet->settings()->xapianIndexDirList;
    } else {
        m_indexDirs = forcedScanDirList;
    }
    m_cacheDir = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
}

void CXapianIndexWorker::startMain()
{
#ifdef WITH_XAPIAN
    static QMutex mainIndexerMutex;
    QMutexLocker locker(&mainIndexerMutex);

    if (!m_cacheDir.isReadable()) return;

    const QString dbFile = m_cacheDir.filePath(QSL("xapian_index"));

    QElapsedTimer tmr;
    tmr.start();

    int dbMode = Xapian::DB_CREATE_OR_OPEN;
    if (m_cleanDB)
        dbMode = Xapian::DB_CREATE_OR_OVERWRITE;

    m_db.reset(new Xapian::WritableDatabase(dbFile.toStdString(), dbMode));
    auto dbCleanup = qScopeGuard([this]{
        m_db->commit();
        m_db->close();
        Q_EMIT finished();
    });

    std::map<std::string,QString> fsFiles;
    for (const auto &dir : qAsConst(m_indexDirs)) {
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
    for (auto it = m_db->allterms_begin(prefix), end = m_db->allterms_end(prefix); it != end; ++it) {
        baseIDs.emplace(*it,QString());
        if (isAborted()) {
            qWarning() << QSL("XapianIndexer: Aborting.");
            return;
        }
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

    tmr.restart();
    std::vector<std::pair<std::string,QString> > deletedFiles;
    std::set_difference(baseIDs.cbegin(),baseIDs.cend(),fsFiles.cbegin(),fsFiles.cend(),
                        std::back_inserter(deletedFiles),
                        [](const auto& s1, const auto& s2){
        return s1.first < s2.first;
    });
    qInfo() << QSL("XapianIndexer: Files to remove from database: %1 (%2 ms).").arg(deletedFiles.size()).arg(tmr.elapsed());

    tmr.restart();
    if (!deletedFiles.empty()) {
        for (const auto& fpair : deletedFiles) {
            m_db->delete_document(fpair.first);
            if (isAborted()) {
                qWarning() << QSL("XapianIndexer: Aborting.");
                return;
            }
        }
        qInfo() << QSL("XapianIndexer: Deleted files was removed from base (%1 ms).").arg(tmr.elapsed());
    }

    tmr.restart();
    if (!newFiles.empty()) {
        QAtomicInteger<int> cnt(0);
        qInfo() << QSL("XapianIndexer: Indexing %1 files...").arg(newFiles.size());
        std::for_each(std::execution::par, newFiles.begin(), newFiles.end(),
                      [this,&cnt](const auto& fpair) {
            if (handleFile(fpair.second))
                cnt++;
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
    if (m_db.isNull())
        return tr("Xapian indexer (initializing)");

    return tr("Xapian indexer (%1 root dirs)").arg(m_indexDirs.count());
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
            CHTMLNode doc(CHTMLParser::parseHTML(textContent));
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

    if (!textContent.isEmpty()) {
        const QByteArray utf8Content = CGenericFuncs::detectDecodeToUtf8(textContent);
        if (!utf8Content.isEmpty()) {
            // ******* Add file to database
            Xapian::TermGenerator generator;
            if (!m_stemLang.empty())
                generator.set_stemmer(Xapian::Stem(m_stemLang));
            generator.set_flags(Xapian::TermGenerator::FLAG_CJK_WORDS);
            generator.set_document(doc);
            generator.index_text(utf8Content.toStdString());
        } else {
            qCritical() << QSL("XapianIndexer: Unable to decode file %1 to UTF-8").arg(filename);
            textContent.clear(); // codepage error
        }
    }

    // Leave empty document for failed or unknown file to avoid rescan.

    QString errMsg;
    try {
        doc.set_data(filename.toStdString());
        doc.add_boolean_term(docID);
        static QMutex dbMutex;
        {
            QMutexLocker locker(&dbMutex);
            m_db->replace_document(docID,doc);
        }
    } catch (const Xapian::Error &err) {
        errMsg = QString::fromStdString(err.get_msg());
    } catch (const std::string &s) {
        errMsg = QString::fromStdString(s);
    } catch (const char *s) {
        errMsg = QString::fromUtf8(s);
    }

    if (!errMsg.isEmpty()) {
        qCritical() << QSL("XapianIndexer: Xapian exception: %1").arg(errMsg);
        textContent.clear(); // xapian database error
    }

    if (!textContent.isEmpty())
        addLoadedRequest(textContent.size());

#endif
    return true;
}
