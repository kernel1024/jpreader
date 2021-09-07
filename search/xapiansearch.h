#ifndef XAPIANSEARCH_H
#define XAPIANSEARCH_H

#include <string>
#include <QObject>
#include <QDir>
#include "abstractthreadedsearch.h"

class CXapianSearch : public CAbstractThreadedSearch
{
    Q_OBJECT
private:
    std::string m_stemLang;
    QDir m_cacheDir;

public:
    explicit CXapianSearch(QObject *parent = nullptr);

public Q_SLOTS:
    void doSearch(const QString &qr, int maxLimit) override;
};

#endif // XAPIANSEARCH_H
