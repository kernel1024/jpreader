#include "baloo5search.h"
#ifdef WITH_BALOO5
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#endif

CBaloo5Search::CBaloo5Search(QObject *parent) : CAbstractThreadedSearch(parent)
{
}

void CBaloo5Search::doSearch(const QString &qr, int maxLimit)
{
#ifdef WITH_BALOO5
    if (working) return;
    working = true;

    Baloo::Query baloo;
    baloo.setSearchString(qr);
    baloo.setLimit(maxLimit);

    Baloo::ResultIterator i = baloo.exec();
    while (i.next()) {
        QString fname = i.filePath();
        if (fname.endsWith('/') || fname.endsWith('\\')) continue;
        emit addHit(fname);
    }
    working = false;
#else
    Q_UNUSED(qr);
    Q_UNUSED(maxLimit);
#endif
    emit finished();
}