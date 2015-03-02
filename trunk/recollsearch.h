#ifndef RECOLLSEARCH_H
#define RECOLLSEARCH_H

#include "abstractthreadedsearch.h"

class CRecollSearch : public CAbstractThreadedSearch
{
    Q_OBJECT
public:
    explicit CRecollSearch(QObject *parent = 0);
    
public slots:
    virtual void doSearch(const QString &qr, int maxLimit);
    
};

#endif // RECOLLSEARCH_H
