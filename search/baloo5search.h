#ifndef CBALOO5SEARCH_H
#define CBALOO5SEARCH_H

#include "abstractthreadedsearch.h"

class CBaloo5Search : public CAbstractThreadedSearch
{
    Q_OBJECT
public:
    explicit CBaloo5Search(QObject *parent = nullptr);

public Q_SLOTS:
    void doSearch(const QString &qr, int maxLimit) override;
};

#endif // CBALOO5SEARCH_H
