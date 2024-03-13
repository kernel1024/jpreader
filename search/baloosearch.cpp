#include "baloosearch.h"
#ifdef WITH_BALOO
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#endif

CBalooSearch::CBalooSearch(QObject *parent) : CAbstractThreadedSearch(parent)
{
}

void CBalooSearch::doSearch(const QString &qr, int maxLimit)
{
#ifdef WITH_BALOO
    if (isWorking()) return;
    setWorking(true);

    Baloo::Query baloo;
    baloo.setSearchString(qr);
    baloo.setLimit(static_cast<uint>(maxLimit));

    Baloo::ResultIterator i = baloo.exec();
    while (i.next()) {
        QString fname = i.filePath();
        if (fname.endsWith(u'/') || fname.endsWith(u'\\')) continue;
        Q_EMIT addHit({ { QSL("jp:fullfilename"), fname } });
    }
    setWorking(false);
#else
    Q_UNUSED(qr);
    Q_UNUSED(maxLimit);
#endif
    Q_EMIT finished();
}
