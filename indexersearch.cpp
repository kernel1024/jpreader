#include <QTextCodec>
#include "indexersearch.h"
#include "globalcontrol.h"
#include "genericfuncs.h"

CIndexerSearch::CIndexerSearch(QObject *parent) :
    QObject(parent)
{
    working = false;
    resultCount = 0;
    m_query = QString();
    indexerSerivce = gSet->settings.searchEngine;
    if ((indexerSerivce == SE_RECOLL) || (indexerSerivce == SE_BALOO5)) {
#ifdef WITH_THREADED_SEARCH
        if (!isValidConfig()) {
            engine = nullptr;
            return;
        }
        auto th = new QThread();
        if (indexerSerivce == SE_RECOLL)
            engine = new CRecollSearch();
        else
            engine = new CBaloo5Search();
        connect(engine,&CAbstractThreadedSearch::addHit,
                this,&CIndexerSearch::auxAddHit,Qt::QueuedConnection);
        connect(engine,&CAbstractThreadedSearch::finished,
                this,&CIndexerSearch::engineFinished,Qt::QueuedConnection);
        connect(this,&CIndexerSearch::startThreadedSearch,
                engine,&CAbstractThreadedSearch::doSearch,Qt::QueuedConnection);
        engine->moveToThread(th);
        th->start();
#endif
    }
}

void CIndexerSearch::doSearch(const QString &searchTerm, const QDir &searchDir)
{
    if (working) return;
    bool useFSSearch = (!searchDir.isRoot() && searchDir.isReadable());

    m_query = searchTerm;
    resultCount = 0;

    gSet->appendSearchHistory(QStringList() << searchTerm);
    working = true;
    searchTimer.start();
    if (useFSSearch) {
        searchInDir(searchDir,m_query);
        engineFinished();
    } else {
        if ((indexerSerivce == SE_BALOO5) || (indexerSerivce == SE_RECOLL)) {
#ifdef WITH_THREADED_SEARCH
            if (isValidConfig())
                emit startThreadedSearch(m_query,gSet->settings.maxSearchLimit);
            else
                engineFinished();
#endif
        } else
            engineFinished();
    }
}

bool CIndexerSearch::isValidConfig()
{
#ifndef WITH_RECOLL
    if (indexerSerivce == SE_RECOLL) return false;
#endif
#ifndef WITH_BALOO5
    if (indexerSerivce == SE_BALOO5) return false;
#endif
    return true;
}

bool CIndexerSearch::isWorking()
{
    return working;
}

int CIndexerSearch::getCurrentIndexerService()
{
    return indexerSerivce;
}

void CIndexerSearch::addHitFS(const QFileInfo &hit, const QString &title,
                              double rel, const QString &snippet)
{
    // get URI and file info
    QString w = hit.absoluteFilePath();
    double nhits = -1.0;
    QString ftitle;
    bool relPercent = false;

    // use indexer file info from search engine (if available)
    if (rel>=0.0) {
        ftitle = title;
        nhits = rel;
        relPercent = true;
    } else
        processFile(w,nhits,ftitle);

    if (nhits<0.01) return;

    QString fileName = hit.fileName();

    if (ftitle.isEmpty())
        ftitle = fileName;

    CStringHash result;

    result[QStringLiteral("Uri")] = QStringLiteral("file://%1").arg(w);
    result[QStringLiteral("FullFilename")] = w;
    result[QStringLiteral("DisplayFilename")] = w;
    result[QStringLiteral("FilePath")] = hit.path();
    result[QStringLiteral("FileSize")] = QStringLiteral("%L1 Kb")
                                         .arg(static_cast<double>(hit.size())/1024.0, 0, 'f', 1);
    result[QStringLiteral("FileSizeNum")] = QString::number(hit.size());
    result[QStringLiteral("OnlyFilename")] = fileName;
    result[QStringLiteral("Dir")] = QDir(hit.dir()).dirName();

    // extract base properties (score, type etc)
    result[QStringLiteral("Score")]=QString::number(nhits,'f',4);
    result[QStringLiteral("MimeT")]=QString();

    QDateTime dtm=hit.lastModified();
    result[QStringLiteral("Time")]=dtm.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))
                                   + QStringLiteral(" (Utc)");

    result[QStringLiteral("dc:title")]=ftitle;
    result[QStringLiteral("Filename")]= fileName;
    result[QStringLiteral("FileTitle")] = fileName;

    if (relPercent)
        result[QStringLiteral("relMode")]=QStringLiteral("percent");
    else
        result[QStringLiteral("relMode")]=QStringLiteral("count");

    if (!snippet.isEmpty())
        result[QStringLiteral("Snip")]=snippet;

    resultCount++;
    emit gotResult(result);
}

void CIndexerSearch::processFile(const QString &filename, double &hitRate, QString &title)
{
    QFileInfo fi(filename);
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) {
        hitRate = -1.0;
        title = fi.fileName();
        return;
    }
    QByteArray fb;
    if (f.size()<50*1024*1024)
        fb = f.readAll();
    else
        fb = f.read(50*1024*1024);
    f.close();

    QTextCodec *cd = detectEncoding(fb);
    QString fc = cd->toUnicode(fb.constData());

    hitRate = calculateHitRate(fc);

    QString mime = detectMIME(fb);
    if (mime.contains(QStringLiteral("html"),Qt::CaseInsensitive)) {
        title = extractFileTitle(fc);
        if (title.isEmpty())
            title = fi.fileName();
    } else {
        title = fi.fileName();
    }

    fb.clear();
    fc.clear();
}

double CIndexerSearch::calculateHitRate(const QString &fc)
{

    QStringList ql = m_query.split(' ');
    double hits = 0.0;
    for(int i=0;i<ql.count();i++)
        hits += fc.count(ql.at(i),Qt::CaseInsensitive);
    return hits;
}

void CIndexerSearch::searchInDir(const QDir &dir, const QString &qr)
{
    static const QStringList anyFile( { QStringLiteral("*") } );

    QFileInfoList fl = dir.entryInfoList(anyFile,QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i=0;i<fl.count();i++)
        if (fl.at(i).isDir() && fl.at(i).isReadable())
            searchInDir(QDir(fl.at(i).absoluteFilePath()),qr);

    fl = dir.entryInfoList(anyFile,QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
    for (int i=0;i<fl.count();i++) {
        if (!(fl.at(i).isFile() && fl.at(i).isReadable())) continue;
        addHitFS(fl.at(i));
    }
}

void CIndexerSearch::auxAddHit(const QString &fileName, const QString &title,
                               double rel, const QString& snippet)
{
    QFileInfo fi(fileName);
    addHitFS(fi,title,rel,snippet);
}

void CIndexerSearch::engineFinished()
{
    if (!working) return;
    working = false;

    CStringHash stats;
    stats[QStringLiteral("Elapsed time")] = QString::number(
                static_cast<double>(searchTimer.elapsed())/1000,'f',3);
    stats[QStringLiteral("Total hits")] = QString::number(resultCount);
    emit searchFinished(stats,m_query);
}
