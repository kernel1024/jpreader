#ifndef ABSTRACTTHREADEDSEARCH_H
#define ABSTRACTTHREADEDSEARCH_H

#include <QObject>
#include <QString>
#include "global/structures.h"


class CAbstractThreadedSearch : public QObject
{
    Q_OBJECT

private:
    bool m_working { false };

protected:
    bool isWorking() const;
    void setWorking(bool working);

public:
    explicit CAbstractThreadedSearch(QObject *parent = nullptr);

Q_SIGNALS:
    void addHit(const CStringHash &meta);
    void finished();
    void errorOccured(const QString& message);

public Q_SLOTS:
    virtual void doSearch(const QString &qr, int maxLimit) = 0;

};

#endif // ABSTRACTTHREADEDSEARCH_H
