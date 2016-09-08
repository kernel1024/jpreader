#ifndef CABSTRACTTHREADEDSEARCH_H
#define CABSTRACTTHREADEDSEARCH_H

#include <QObject>
#include <QString>


class CAbstractThreadedSearch : public QObject
{
    Q_OBJECT

protected:
    bool working;

public:
    explicit CAbstractThreadedSearch(QObject *parent = 0);

signals:
    void addHit(const QString &fileName);
    void addHitFull(const QString &fileName, const QString &title, double rel);
    void finished();

public slots:
    virtual void doSearch(const QString &qr, int maxLimit);

};

#endif // CABSTRACTTHREADEDSEARCH_H
