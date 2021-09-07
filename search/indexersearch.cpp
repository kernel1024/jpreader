#include <QTextCodec>
#include <QThread>

#include "indexersearch.h"
#include "global/control.h"
#include "global/history.h"
#include "utils/genericfuncs.h"

#include "baloo5search.h"
#include "recollsearch.h"
#include "xapiansearch.h"
#include "defaultsearch.h"

CIndexerSearch::CIndexerSearch(QObject *parent) :
    QObject(parent)
{
    m_engineMode = gSet->settings()->searchEngine;
}

void CIndexerSearch::doSearch(const QString &searchTerm, const QDir &searchDir)
{
    if (m_working) return;
    m_working = true;

    bool isSearchDirValid = (!searchDir.isRoot() && searchDir.isReadable());
    if (!isValidConfig()) {
        Q_EMIT gotError(tr("Search: incorrect search engine specified."));
        engineFinished();
        return;
    }
    setupEngine();
    if (m_engineMode == CStructures::seNone) {
        if (isSearchDirValid) {
            auto *fs = qobject_cast<CDefaultSearch *>(m_engine.data());
            if (fs)
                fs->setSearchDir(searchDir);
        } else {
            Q_EMIT gotError(tr("Search: incorrect directory specified for default file search."));
            engineFinished();
            return;
        }
    }

    m_query = searchTerm;
    m_resultCount = 0;
    m_searchTimer.start();
    gSet->history()->appendSearchHistory({ searchTerm });

    Q_EMIT startThreadedSearch(m_query,gSet->settings()->maxSearchLimit);
}

bool CIndexerSearch::isValidConfig()
{
#ifndef WITH_RECOLL
    if (m_engineMode == CStructures::seRecoll) return false;
#endif
#ifndef WITH_BALOO5
    if (m_engineMode == CStructures::seBaloo5) return false;
#endif
#ifndef WITH_XAPIAN
    if (m_engineMode == CStructures::seXapian) return false;
#endif
    return true;
}

bool CIndexerSearch::isWorking() const
{
    return m_working;
}

CStructures::SearchEngine CIndexerSearch::getCurrentIndexerService()
{
    return m_engineMode;
}

void CIndexerSearch::addHit(const CStringHash &meta)
{
    CStringHash result = meta;

    QFileInfo fi(result.value(QSL("jp:fullfilename")));
    if (!result.contains(QSL("url"))) {
        result[QSL("url")] = QSL("file://%1").arg(fi.absoluteFilePath());
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

    m_resultCount++;
    Q_EMIT gotResult(result);
}

void CIndexerSearch::engineError(const QString &message)
{
    Q_EMIT gotError(message);
}

void CIndexerSearch::processFile(const QString &filename, int &hitRate, QString &title)
{
    const QFileInfo fi(filename);
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

    const QString mime = CGenericFuncs::detectMIME(fb);
    const QString fc = CGenericFuncs::detectDecodeToUnicode(fb.constData());
    hitRate = calculateHitRate(fc);
    if (mime.contains(QSL("html"),Qt::CaseInsensitive)) {
        title = CGenericFuncs::extractFileTitle(fc);
        if (title.isEmpty())
            title = fi.fileName();
    } else {
        title = fi.fileName();
    }
}

int CIndexerSearch::calculateHitRate(const QString &fc)
{
    const QStringList ql = m_query.split(u' ');
    int hits = 0;
    for(const auto &s : ql)
        hits += fc.count(s,Qt::CaseInsensitive);
    return hits;
}

void CIndexerSearch::setupEngine()
{
    auto *th = new QThread();

    switch (m_engineMode) {
        case CStructures::seXapian: m_engine.reset(new CXapianSearch()); break;
        case CStructures::seRecoll: m_engine.reset(new CRecollSearch()); break;
        case CStructures::seBaloo5: m_engine.reset(new CBaloo5Search()); break;
        case CStructures::seNone:   m_engine.reset(new CDefaultSearch()); break;
    }

    m_engine->moveToThread(th);
    connect(m_engine.data(),&CAbstractThreadedSearch::addHit,
            this,&CIndexerSearch::addHit,Qt::QueuedConnection);
    connect(m_engine.data(),&CAbstractThreadedSearch::errorOccured,
            this,&CIndexerSearch::engineError,Qt::QueuedConnection);
    connect(m_engine.data(),&CAbstractThreadedSearch::finished,
            this,&CIndexerSearch::engineFinished,Qt::QueuedConnection);
    connect(this,&CIndexerSearch::startThreadedSearch,
            m_engine.data(),&CAbstractThreadedSearch::doSearch,Qt::QueuedConnection);
    connect(m_engine.data(),&CAbstractThreadedSearch::destroyed,th,&QThread::quit);
    connect(th,&QThread::finished,th,&QThread::deleteLater);
    th->setObjectName(QSL("%1").arg(QString::fromLatin1(m_engine->metaObject()->className())));
    th->start();
}

void CIndexerSearch::engineFinished()
{
    if (!m_working) return;
    m_working = false;
    const double oneK = 1000.0;

    CStringHash stats;
    stats[QSL("jp:elapsedtime")] = QString::number(
                static_cast<double>(m_searchTimer.elapsed())/oneK,'f',3);
    stats[QSL("jp:totalhits")] = QString::number(m_resultCount);
    Q_EMIT searchFinished(stats,m_query);
}
