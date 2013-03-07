#ifndef RECOLLSEARCH_H
#define RECOLLSEARCH_H

#include <QObject>
#include <QString>

class CRecollSearch : public QObject
{
    Q_OBJECT
public:
    explicit CRecollSearch(QObject *parent = 0);

private:
    bool working;
    
signals:
    void addHit(const QString &fileName);
    void finished();
    
public slots:
    void doSearch(const QString &qr, int maxLimit);
    
};

#endif // RECOLLSEARCH_H
