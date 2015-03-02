//#include <Baloo/Query>
#include "baloo/query.h"
//#include <Baloo/ResultIterator>
#include "baloo/resultiterator.h"
#include "baloo5search.h"

CBaloo5Search::CBaloo5Search(QObject *parent) : CAbstractThreadedSearch(parent)
{
}

void CBaloo5Search::doSearch(const QString &qr, int maxLimit)
{
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
    emit finished();
}
