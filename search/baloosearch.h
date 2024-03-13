#ifndef CBALOOSEARCH_H
#define CBALOOSEARCH_H

#include "abstractthreadedsearch.h"

class CBalooSearch : public CAbstractThreadedSearch
{
    Q_OBJECT
public:
    explicit CBalooSearch(QObject *parent = nullptr);

public Q_SLOTS:
    void doSearch(const QString &qr, int maxLimit) override;
};

#endif // CBALOOSEARCH_H
