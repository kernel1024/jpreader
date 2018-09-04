#ifndef RECOLLSEARCH_H
#define RECOLLSEARCH_H

#include "abstractthreadedsearch.h"

class CRecollSearch : public CAbstractThreadedSearch
{
    Q_OBJECT
public:
    explicit CRecollSearch(QObject *parent = nullptr);

private:
    QString strFromBase64(const QString &src);
    
public slots:
    virtual void doSearch(const QString &qr, int maxLimit);
    
};

#endif // RECOLLSEARCH_H
