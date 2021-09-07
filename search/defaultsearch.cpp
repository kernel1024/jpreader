#include <QFileInfo>
#include <QDir>
#include <QFile>

#include "defaultsearch.h"
#include "utils/genericfuncs.h"

CDefaultSearch::CDefaultSearch(QObject *parent)
    : CAbstractThreadedSearch(parent)
{
}

void CDefaultSearch::setSearchDir(const QDir &newSearchDir)
{
    m_searchDir = newSearchDir;
}

void CDefaultSearch::searchInDir(const QDir &dir, const QString &qr)
{
    if (m_resultCounter>m_maxLimit) return;

    static const QStringList anyFile( { QSL("*") } );

    const QFileInfoList dl = dir.entryInfoList(anyFile,QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &fi : dl) {
        if (m_resultCounter>m_maxLimit) return;
        if (fi.isDir() && fi.isReadable())
            searchInDir(QDir(fi.filePath()),qr);
    }

    const QFileInfoList fl = dir.entryInfoList(anyFile,QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
    for (const auto &fi : fl) {
        if (m_resultCounter>m_maxLimit) return;
        if (!(fi.isFile() && fi.isReadable())) continue;

        QFile f(fi.filePath());
        if (f.open(QIODevice::ReadOnly)) {
            if (CGenericFuncs::detectDecodeToUnicode(f.readAll()).contains(qr)) {
                Q_EMIT addHit({ { QSL("jp:fullfilename"),fi.absoluteFilePath() } });
                m_resultCounter++;
            }
            f.close();
        }
    }
}

void CDefaultSearch::doSearch(const QString &qr, int maxLimit)
{
    if (isWorking()) return;
    setWorking(true);

    m_maxLimit = maxLimit;
    m_resultCounter = 0;
    searchInDir(m_searchDir,qr);

    setWorking(false);

    Q_EMIT finished();
}
