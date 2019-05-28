#ifndef CABSTRACTTHREADEDSEARCH_H
#define CABSTRACTTHREADEDSEARCH_H

#include <QObject>
#include <QString>
#include "structures.h"


class CAbstractThreadedSearch : public QObject
{
    Q_OBJECT

private:
    bool m_working { false };

protected:
    bool isWorking();
    void setWorking(bool working);

public:
    explicit CAbstractThreadedSearch(QObject *parent = nullptr);

signals:
    void addHit(const CStringHash &meta);
    void finished();

public slots:
    virtual void doSearch(const QString &qr, int maxLimit) = 0;

};

#endif // CABSTRACTTHREADEDSEARCH_H
