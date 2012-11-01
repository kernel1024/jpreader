#include "nepomuksearch.h"
#include "globalcontrol.h"
#include "genericfuncs.h"

CNepomukSearch::CNepomukSearch(QObject *parent) :
    QObject(parent)
{
    working = false;
    result = QBResult();
    query = QString();
    connect(engine.dirLister(),SIGNAL(completed()),this,SLOT(nepomukFinished()));
    connect(engine.dirLister(),SIGNAL(newItems(KFileItemList)),this,SLOT(nepomukNewItems(KFileItemList)));
}

void CNepomukSearch::doSearch(const QString &searchTerm, const QDir &searchDir)
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
        nepomukFinished();
    } else {
#ifdef WITH_NEPOMUK
        Nepomuk::Query::LiteralTerm term(query);
        Nepomuk::Query::FileQuery qr(term);

        KUrl ndir = qr.toSearchUrl();
        engine.dirLister()->openUrl(ndir);
#else
        nepomukFinished();
#endif
    }
}

void CNepomukSearch::addHit(const KFileItem &hit)
{
    if (result.snippets.count()>gSet->maxLimit) return;
    // Add only files (no feeds, cached pages etc...)
    if (!hit.isFile()) return;
    if (hit.localPath().isEmpty()) return;

    // get URI and file info
    QString fname = hit.localPath();
    double nhits = calculateHitRate(fname);

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

    result.snippets.last()["dc:title"]=wf.fileName();
    result.snippets.last()["Filename"]=fname;
    result.snippets.last()["FileTitle"] = wf.fileName();
}

void CNepomukSearch::addHitFS(const QFileInfo &hit)
{
    // get URI and file info
    QString w = hit.absoluteFilePath();
    double nhits=calculateHitRate(w);
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

    result.snippets.last()["Uri"] = w;
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

    result.snippets.last()["dc:title"]=hit.fileName();
    result.snippets.last()["Filename"]=hit.fileName();
    result.snippets.last()["FileTitle"] = hit.fileName();
}

double CNepomukSearch::calculateHitRate(const QString &filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) return 0.0;
    QByteArray fb;
    if (f.size()<50*1024*1024)
        fb = f.readAll();
    else
        fb = f.read(50*1024*1024);
    f.close();

    QTextCodec *cd = detectEncoding(fb);
    QString fc = cd->toUnicode(fb.constData());

    double hits = fc.count(query,Qt::CaseInsensitive);
    return hits;
}

void CNepomukSearch::searchInDir(const QDir &dir, const QString &qr)
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

void CNepomukSearch::nepomukNewItems(const KFileItemList &items)
{
    if (!working) return;
    for (int i=0;i<items.count();i++)
        addHit(items.at(i));
}

void CNepomukSearch::nepomukFinished()
{
    if (!working) return;
    working = false;
    if (!result.snippets.isEmpty()) {
        result.stats["Elapsed time"] = QString("%1").arg(((double)searchTimer.elapsed())/1000,1,'f',3);
        result.stats["Total hits"] = QString("%1").arg(result.snippets.count());
        result.presented = true;
    }
    emit searchFinished();
}
