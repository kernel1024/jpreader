#include <QTextCodec>
#include "indexersearch.h"
#include "globalcontrol.h"
#include "genericfuncs.h"

CIndexerSearch::CIndexerSearch(QObject *parent) :
    QObject(parent)
{
    indexerSerivce = gSet->settings.searchEngine;
    if ((indexerSerivce == seRecoll) || (indexerSerivce == seBaloo5)) {
#ifdef WITH_THREADED_SEARCH
        if (!isValidConfig()) {
            engine = nullptr;
            return;
        }
        auto th = new QThread();
        if (indexerSerivce == seRecoll) {
            engine = new CRecollSearch();
        } else {
            engine = new CBaloo5Search();
        }
        connect(engine,&CAbstractThreadedSearch::addHit,
                this,&CIndexerSearch::addHit,Qt::QueuedConnection);
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
        if ((indexerSerivce == seBaloo5) || (indexerSerivce == seRecoll)) {
#ifdef WITH_THREADED_SEARCH
            if (isValidConfig()) {
                emit startThreadedSearch(m_query,gSet->settings.maxSearchLimit);
            } else {
                engineFinished();
            }
#endif
        } else {
            engineFinished();
        }
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

SearchEngine CIndexerSearch::getCurrentIndexerService()
{
    return indexerSerivce;
}

void CIndexerSearch::addHit(const CStringHash &meta)
{
    CStringHash result = meta;

    QFileInfo fi(result.value(QStringLiteral("jp:fullfilename")));
    if (!result.contains(QStringLiteral("url"))) {
        result[QStringLiteral("uri")] = QStringLiteral("file://%1")
                                        .arg(fi.absoluteFilePath());
    }

    if (!result.contains(QStringLiteral("relevancyrating"))) {
        int nhits = -1;
        QString ftitle;

        processFile(fi.absoluteFilePath(),nhits,ftitle);
        if (nhits<0) return;

        result[QStringLiteral("relevancyrating")]=QString::number(nhits);
        result[QStringLiteral("title")]=ftitle;
    }

    result[QStringLiteral("jp:filepath")] = fi.path();
    result[QStringLiteral("jp:dir")] = fi.absoluteDir().dirName();

    if (!result.contains(QStringLiteral("fbytes")))
        result[QStringLiteral("fbytes")] = QString::number(fi.size());

    if (!result.contains(QStringLiteral("filename")))
        result[QStringLiteral("filename")] = fi.fileName();

    if (!result.contains(QStringLiteral("title")))
        result[QStringLiteral("title")]=fi.fileName();

    resultCount++;
    emit gotResult(result);
}

void CIndexerSearch::processFile(const QString &filename, int &hitRate, QString &title)
{
    QFileInfo fi(filename);
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) {
        hitRate = -1;
        title = fi.fileName();
        return;
    }
    QByteArray fb;
    if (f.size()<maxSearchFileSize) {
        fb = f.readAll();
    } else {
        fb = f.read(maxSearchFileSize);
    }
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

int CIndexerSearch::calculateHitRate(const QString &fc)
{

    QStringList ql = m_query.split(' ');
    int hits = 0;
    for(int i=0;i<ql.count();i++)
        hits += fc.count(ql.at(i),Qt::CaseInsensitive);
    return hits;
}

void CIndexerSearch::searchInDir(const QDir &dir, const QString &qr)
{
    static const QStringList anyFile( { QStringLiteral("*") } );

    QFileInfoList fl = dir.entryInfoList(anyFile,QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i=0;i<fl.count();i++) {
        if (fl.at(i).isDir() && fl.at(i).isReadable())
            searchInDir(QDir(fl.at(i).absoluteFilePath()),qr);
    }

    fl = dir.entryInfoList(anyFile,QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
    for (int i=0;i<fl.count();i++) {
        if (!(fl.at(i).isFile() && fl.at(i).isReadable())) continue;
        addHit({ { QStringLiteral("jp:fullfilename"),fl.at(i).absoluteFilePath() } });
    }
}

void CIndexerSearch::engineFinished()
{
    if (!working) return;
    working = false;
    const int oneK = 1000;

    CStringHash stats;
    stats[QStringLiteral("jp:elapsedtime")] = QString::number(
                static_cast<double>(searchTimer.elapsed())/oneK,'f',3);
    stats[QStringLiteral("jp:totalhits")] = QString::number(resultCount);
    emit searchFinished(stats,m_query);
}
