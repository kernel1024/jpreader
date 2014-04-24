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
    indexerSerivce = gSet->searchEngine;
    if (indexerSerivce == SE_NEPOMUK) {
#ifdef WITH_NEPOMUK
        connect(engine.dirLister(),SIGNAL(completed()),this,SLOT(engineFinished()));
        connect(engine.dirLister(),SIGNAL(newItems(KFileItemList)),this,SLOT(nepomukNewItems(KFileItemList)));
#endif
    } else if (indexerSerivce == SE_RECOLL) {
#ifdef WITH_RECOLL
        QThread* th = new QThread();
        recollEngine = new CRecollSearch();
        connect(recollEngine,SIGNAL(addHit(QString)),this,SLOT(auxAddHit(QString)),Qt::QueuedConnection);
        connect(recollEngine,SIGNAL(finished()),this,SLOT(engineFinished()),Qt::QueuedConnection);
        connect(this,SIGNAL(recollStartSearch(QString,int)),
                recollEngine,SLOT(doSearch(QString,int)),Qt::QueuedConnection);
        recollEngine->moveToThread(th);
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
        if (indexerSerivce == SE_NEPOMUK) {
#ifdef WITH_NEPOMUK
#if WITH_NEPOMUK == 1
            Nepomuk::Query::LiteralTerm term(query);
            Nepomuk::Query::FileQuery qr(term);
#else
            Nepomuk2::Query::LiteralTerm term(query);
            Nepomuk2::Query::FileQuery qr(term);
#endif
            KUrl ndir = qr.toSearchUrl();
            engine.dirLister()->openUrl(ndir);
#else
            engineFinished();
#endif
        } else if (indexerSerivce == SE_RECOLL) {
#ifdef WITH_RECOLL
            emit recollStartSearch(query,gSet->maxLimit);
#else
            engineFinished();
#endif
        } else
            engineFinished();
    }
}

bool CIndexerSearch::isValidConfig()
{
#ifndef WITH_NEPOMUK
    if (indexerSerivce == SE_NEPOMUK) return false;
#endif
#ifndef WITH_RECOLL
    if (indexerSerivce == SE_RECOLL) return false;
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

#ifdef WITH_NEPOMUK
void CIndexerSearch::addHit(const KFileItem &hit)
{
    if (result.snippets.count()>gSet->maxLimit) return;
    // Add only files (no feeds, cached pages etc...)
    if (!hit.isFile()) return;
    if (hit.localPath().isEmpty()) return;

    // get URI and file info
    QString fname = hit.localPath();
    double nhits = 0.0;
    QString title;
    processFile(fname,nhits,title);

    // scan existing snippets and add new empty snippet
    for(int i=0;i<result.snippets.count();i++) {
        if (result.snippets[i]["FullFilename"]==fname) {
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

    result.snippets.last()["Uri"] = QUrl::fromLocalFile(fname).toString();
    QFileInfo wf(fname);
    result.snippets.last()["FullFilename"] = fname;
    result.snippets.last()["DisplayFilename"] = fname;
    result.snippets.last()["FilePath"] = wf.filePath();
    if (wf.exists()) {
        double sz = (double)(wf.size()) / 1024;
        result.snippets.last()["FileSize"] = QString("%L1 Kb").arg(sz, 0, 'f', 1);
        result.snippets.last()["FileSizeNum"] = QString("%1").arg(wf.size());
    } else {
        result.snippets.last()["FullFilename"] = "not_found";
        result.snippets.last()["FileSize"] = "0";
        result.snippets.last()["FileSizeNum"] = "0";
    }
    result.snippets.last()["OnlyFilename"] = wf.fileName();
    result.snippets.last()["Dir"] = QDir(wf.dir()).dirName();

    // extract base properties (score, type etc)
    result.snippets.last()["Src"]="Files";
    result.snippets.last()["Score"]=QString("%1").arg(nhits,0,'f',4);
    result.snippets.last()["MimeT"]=hit.mimetype();
    result.snippets.last()["MimeType"]=hit.mimetype();
    result.snippets.last()["Type"]="File";

    result.snippets.last()["Time"]=hit.time(KFileItem::ModificationTime).toString("yyyy-MM-dd hh:mm:ss")+" (Utc)";

    result.snippets.last()["dc:title"]=title;
    result.snippets.last()["Filename"]=fname;
    result.snippets.last()["FileTitle"] = wf.fileName();
}

void CIndexerSearch::nepomukNewItems(const KFileItemList &items)
{
    if (!working) return;
    for (int i=0;i<items.count();i++)
        addHit(items.at(i));
}
#endif

void CIndexerSearch::addHitFS(const QFileInfo &hit)
{
    // get URI and file info
    QString w = hit.absoluteFilePath();
    double nhits = 0.0;
    QString title;
    processFile(w,nhits,title);
    if (nhits<1.0) return;

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
    result.snippets.last()["FilePath"] = hit.filePath();
    double sz = (double)(hit.size()) / 1024;
    result.snippets.last()["FileSize"] = QString("%L1 Kb").arg(sz, 0, 'f', 1);
    result.snippets.last()["FileSizeNum"] = QString("%1").arg(hit.size());
    result.snippets.last()["OnlyFilename"] = hit.fileName();
    result.snippets.last()["Dir"] = QDir(hit.dir()).dirName();

    // extract base properties (score, type etc)
    result.snippets.last()["Src"]=tr("Files");
    result.snippets.last()["Score"]=QString("%1").arg(nhits,0,'f',4);
    result.snippets.last()["MimeT"]="";
    result.snippets.last()["Type"]=tr("File");

    QDateTime dtm=hit.lastModified();
    result.snippets.last()["Time"]=dtm.toString("yyyy-MM-dd hh:mm:ss")+" (Utc)";

    result.snippets.last()["dc:title"]=title;
    result.snippets.last()["Filename"]=hit.fileName();
    result.snippets.last()["FileTitle"] = hit.fileName();
}

void CIndexerSearch::processFile(const QString &filename, double &hitRate, QString &title)
{
    QFileInfo fi(filename);
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) {
        hitRate = 0.0;
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

QString CIndexerSearch::extractFileTitle(const QString& fc)
{
    int pos;
    int start = -1;
    int stop = -1;
    if ((pos = fc.indexOf(QRegExp("<title {0,}>",Qt::CaseInsensitive))) != -1) {
        start = pos;
        if ((pos = fc.indexOf(QRegExp("</title {0,}>", Qt::CaseInsensitive))) != -1) {
            stop = pos;
            if (stop>start) {
                if ((stop-start)>255)
                    stop = start + 255;
                QString s = fc.mid(start,stop-start);
                s.remove(QRegExp("^<title {0,}>",Qt::CaseInsensitive));
                s.remove("\r");
                s.remove("\n");
                return s;
            }
        }
    }
    return QString();
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
    QFileInfo fi(fileName);
    addHitFS(fi);
}

void CIndexerSearch::engineFinished()
{
    if (!working) return;
    working = false;
    if (!result.snippets.isEmpty()) {
        result.stats["Elapsed time"] = QString("%1").arg(((double)searchTimer.elapsed())/1000,1,'f',3);
        result.stats["Total hits"] = QString("%1").arg(result.snippets.count());
        result.presented = true;
    }
    emit searchFinished(result,query);
}
