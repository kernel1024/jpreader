#include <QTextCodec>
#include "indexersearch.h"
#include "globalcontrol.h"
#include "genericfuncs.h"

CIndexerSearch::CIndexerSearch(QObject *parent) :
    QObject(parent)
{
    working = false;
    result = QBResult();
    query = QString();
    indexerSerivce = gSet->settings.searchEngine;
    if ((indexerSerivce == SE_RECOLL) || (indexerSerivce == SE_BALOO5)) {
#ifdef WITH_THREADED_SEARCH
        if (!isValidConfig()) {
            engine = nullptr;
            return;
        }
        QThread* th = new QThread();
        if (indexerSerivce == SE_RECOLL)
            engine = new CRecollSearch();
        else
            engine = new CBaloo5Search();
        connect(engine,&CAbstractThreadedSearch::addHit,
                this,&CIndexerSearch::auxAddHit,Qt::QueuedConnection);
        connect(engine,&CAbstractThreadedSearch::addHitFull,
                this,&CIndexerSearch::auxAddHitFull,Qt::QueuedConnection);
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

    result = QBResult();
    query = searchTerm;

    gSet->appendSearchHistory(QStringList() << searchTerm);
    working = true;
    searchTimer.start();
    if (useFSSearch) {
        searchInDir(searchDir,query);
        engineFinished();
    } else {
        if ((indexerSerivce == SE_BALOO5) || (indexerSerivce == SE_RECOLL)) {
#ifdef WITH_THREADED_SEARCH
            if (isValidConfig())
                emit startThreadedSearch(query,gSet->settings.maxSearchLimit);
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

void CIndexerSearch::addHitFS(const QFileInfo &hit, const QString &title, double rel)
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

    // scan existing snippets and add new empty snippet
    for(int i=0;i<result.snippets.count();i++) {
        if (result.snippets[i]["Uri"]==w) {
            if ((result.snippets[i]["Score"]).toDouble()<=nhits)
                return; // skip same hit with lower score, leave hit with higher score
            else {
                // remove old hit with lower score
                result.snippets.removeAt(i);
                break;
            }
        }
    }
    result.snippets.append(QStrHash());

    result.snippets.last()["Uri"] = QString("file://%1").arg(w);
    result.snippets.last()["FullFilename"] = w;
    result.snippets.last()["DisplayFilename"] = w;
    result.snippets.last()["FilePath"] = hit.path();
    result.snippets.last()["FileSize"] = QString("%L1 Kb")
                                         .arg(static_cast<double>(hit.size())/1024.0, 0, 'f', 1);
    result.snippets.last()["FileSizeNum"] = QString("%1").arg(hit.size());
    result.snippets.last()["OnlyFilename"] = fileName;
    result.snippets.last()["Dir"] = QDir(hit.dir()).dirName();

    // extract base properties (score, type etc)
    result.snippets.last()["Src"]=tr("Files");
    result.snippets.last()["Score"]=QString("%1").arg(nhits,0,'f',4);
    result.snippets.last()["MimeT"]="";
    result.snippets.last()["Type"]=tr("File");

    QDateTime dtm=hit.lastModified();
    result.snippets.last()["Time"]=dtm.toString("yyyy-MM-dd hh:mm:ss")+" (Utc)";

    result.snippets.last()["dc:title"]=ftitle;
    result.snippets.last()["Filename"]= fileName;
    result.snippets.last()["FileTitle"] = fileName;

    if (relPercent)
        result.snippets.last()["relMode"]=tr("percent");
    else
        result.snippets.last()["relMode"]=tr("count");
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
    if (mime.contains("html",Qt::CaseInsensitive)) {
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

    QStringList ql = query.split(' ');
    double hits = 0.0;
    for(int i=0;i<ql.count();i++)
        hits += fc.count(ql.at(i),Qt::CaseInsensitive);
    return hits;
}

void CIndexerSearch::searchInDir(const QDir &dir, const QString &qr)
{
    QFileInfoList fl = dir.entryInfoList(QStringList() << "*",QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i=0;i<fl.count();i++)
        if (fl.at(i).isDir() && fl.at(i).isReadable())
            searchInDir(QDir(fl.at(i).absoluteFilePath()),qr);

    fl = dir.entryInfoList(QStringList() << "*",QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
    for (int i=0;i<fl.count();i++) {
        if (!(fl.at(i).isFile() && fl.at(i).isReadable())) continue;
        addHitFS(fl.at(i));
    }
}

void CIndexerSearch::auxAddHit(const QString &fileName)
{
    auxAddHitFull(fileName, QString(), -1.0);
}

void CIndexerSearch::auxAddHitFull(const QString &fileName, const QString &title, double rel)
{
    QFileInfo fi(fileName);
    addHitFS(fi,title,rel);
}

void CIndexerSearch::engineFinished()
{
    if (!working) return;
    working = false;
    if (!result.snippets.isEmpty()) {
        result.stats["Elapsed time"] = QString("%1")
                                       .arg((static_cast<double>(searchTimer.elapsed()))/1000,1,'f',3);
        result.stats["Total hits"] = QString("%1").arg(result.snippets.count());
        result.presented = true;
    }
    emit searchFinished(result,query);
}

QBResult::QBResult()
{
    presented=false;
    stats.clear();
    snippets.clear();
    sortMode = -2;
}

QBResult::QBResult(const QBResult &other)
{
    presented = other.presented;
    stats=other.stats;
    sortMode=other.sortMode;
    snippets.clear();
    foreach (const QStrHash &h, other.snippets)
        snippets.append(h);
}

QBResult &QBResult::operator=(const QBResult &other)
{
    presented = other.presented;
    stats=other.stats;
    sortMode=other.sortMode;
    snippets.clear();
    foreach (const QStrHash &h, other.snippets)
        snippets.append(h);

    return *this;
}
