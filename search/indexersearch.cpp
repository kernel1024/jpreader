#include <QTextCodec>
#include <QThread>

#include "indexersearch.h"
#include "global/control.h"
#include "global/history.h"
#include "utils/genericfuncs.h"

CIndexerSearch::CIndexerSearch(QObject *parent) :
    QObject(parent)
{
    indexerSerivce = gSet->settings()->searchEngine;
    if ((indexerSerivce == CStructures::seRecoll) || (indexerSerivce == CStructures::seBaloo5)) {
#ifdef WITH_THREADED_SEARCH
        if (!isValidConfig()) {
            engine.reset(nullptr);
            return;
        }
        auto *th = new QThread();
        if (indexerSerivce == CStructures::seRecoll) {
            engine.reset(new CRecollSearch());
        } else {
            engine.reset(new CBaloo5Search());
        }
        engine->moveToThread(th);
        connect(engine.data(),&CAbstractThreadedSearch::addHit,
                this,&CIndexerSearch::addHit,Qt::QueuedConnection);
        connect(engine.data(),&CAbstractThreadedSearch::finished,
                this,&CIndexerSearch::engineFinished,Qt::QueuedConnection);
        connect(this,&CIndexerSearch::startThreadedSearch,
                engine.data(),&CAbstractThreadedSearch::doSearch,Qt::QueuedConnection);
        connect(engine.data(),&CAbstractThreadedSearch::destroyed,th,&QThread::quit);
        connect(th,&QThread::finished,th,&QThread::deleteLater);
        th->setObjectName(QSL("%1").arg(QString::fromLatin1(engine->metaObject()->className())));
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

    gSet->history()->appendSearchHistory({ searchTerm });
    working = true;
    searchTimer.start();
    if (useFSSearch) {
        searchInDir(searchDir,m_query);
        engineFinished();
    } else {
        if ((indexerSerivce == CStructures::seBaloo5) || (indexerSerivce == CStructures::seRecoll)) {
#ifdef WITH_THREADED_SEARCH
            if (isValidConfig()) {
                Q_EMIT startThreadedSearch(m_query,gSet->settings()->maxSearchLimit);
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
    if (indexerSerivce == seRecoll) return false;
#endif
#ifndef WITH_BALOO5
    if (indexerSerivce == seBaloo5) return false;
#endif
    return true;
}

bool CIndexerSearch::isWorking() const
{
    return working;
}

CStructures::SearchEngine CIndexerSearch::getCurrentIndexerService()
{
    return indexerSerivce;
}

void CIndexerSearch::addHit(const CStringHash &meta)
{
    CStringHash result = meta;

    QFileInfo fi(result.value(QSL("jp:fullfilename")));
    if (!result.contains(QSL("url"))) {
        result[QSL("uri")] = QSL("file://%1").arg(fi.absoluteFilePath());
    }

    if (!result.contains(QSL("relevancyrating"))) {
        int nhits = -1;
        QString ftitle;

        processFile(fi.absoluteFilePath(),nhits,ftitle);
        if (nhits<0) return;

        result[QSL("relevancyrating")]=QString::number(nhits);
        result[QSL("title")]=ftitle;
    }

    result[QSL("jp:filepath")] = fi.path();
    result[QSL("jp:dir")] = fi.absoluteDir().dirName();

    if (!result.contains(QSL("fbytes")))
        result[QSL("fbytes")] = QString::number(fi.size());

    if (!result.contains(QSL("filename")))
        result[QSL("filename")] = fi.fileName();

    if (!result.contains(QSL("title")))
        result[QSL("title")]=fi.fileName();

    resultCount++;
    Q_EMIT gotResult(result);
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
    if (f.size()<CDefaults::maxSearchFileSize) {
        fb = f.readAll();
    } else {
        fb = f.read(CDefaults::maxSearchFileSize);
    }
    f.close();

    QTextCodec *cd = CGenericFuncs::detectEncoding(fb);
    QString fc = cd->toUnicode(fb.constData());

    hitRate = calculateHitRate(fc);

    QString mime = CGenericFuncs::detectMIME(fb);
    if (mime.contains(QSL("html"),Qt::CaseInsensitive)) {
        title = CGenericFuncs::extractFileTitle(fc);
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

    QStringList ql = m_query.split(u' ');
    int hits = 0;
    for(int i=0;i<ql.count();i++)
        hits += fc.count(ql.at(i),Qt::CaseInsensitive);
    return hits;
}

void CIndexerSearch::searchInDir(const QDir &dir, const QString &qr)
{
    static const QStringList anyFile( { QSL("*") } );

    QFileInfoList fl = dir.entryInfoList(anyFile,QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i=0;i<fl.count();i++) {
        if (fl.at(i).isDir() && fl.at(i).isReadable())
            searchInDir(QDir(fl.at(i).absoluteFilePath()),qr);
    }

    fl = dir.entryInfoList(anyFile,QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
    for (int i=0;i<fl.count();i++) {
        if (!(fl.at(i).isFile() && fl.at(i).isReadable())) continue;
        addHit({ { QSL("jp:fullfilename"),fl.at(i).absoluteFilePath() } });
    }
}

void CIndexerSearch::engineFinished()
{
    if (!working) return;
    working = false;
    const double oneK = 1000.0;

    CStringHash stats;
    stats[QSL("jp:elapsedtime")] = QString::number(
                static_cast<double>(searchTimer.elapsed())/oneK,'f',3);
    stats[QSL("jp:totalhits")] = QString::number(resultCount);
    Q_EMIT searchFinished(stats,m_query);
}
