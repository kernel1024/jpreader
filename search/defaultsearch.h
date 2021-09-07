#ifndef DEFAULTSEARCH_H
#define DEFAULTSEARCH_H

#include <QObject>
#include <QDir>
#include <QString>
#include "abstractthreadedsearch.h"

class CDefaultSearch : public CAbstractThreadedSearch
{
    Q_OBJECT
public:
    explicit CDefaultSearch(QObject *parent = nullptr);

    void setSearchDir(const QDir &newSearchDir);

private:
    QDir m_searchDir;
    int m_resultCounter { 0 };
    int m_maxLimit { 0 };

    void searchInDir(const QDir &dir, const QString &qr);

public Q_SLOTS:
    void doSearch(const QString &qr, int maxLimit) override;
};

#endif // DEFAULTSEARCH_H
