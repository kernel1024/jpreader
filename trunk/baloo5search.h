#ifndef CBALOO5SEARCH_H
#define CBALOO5SEARCH_H

#include "abstractthreadedsearch.h"

class CBaloo5Search : public CAbstractThreadedSearch
{
    Q_OBJECT
public:
    explicit CBaloo5Search(QObject *parent = 0);

signals:

public slots:
    virtual void doSearch(const QString &qr, int maxLimit);
};

#endif // CBALOO5SEARCH_H
