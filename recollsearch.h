#ifndef RECOLLSEARCH_H
#define RECOLLSEARCH_H

#include <QProcess>
#include "abstractthreadedsearch.h"

class CRecollSearch : public CAbstractThreadedSearch
{
    Q_OBJECT
public:
    explicit CRecollSearch(QObject *parent = nullptr);

private:
    QString strFromBase64(const QString &src);

private slots:
    void recollReadyRead();
    void recollFinished(int exitCode, QProcess::ExitStatus exitStatus);
    
public slots:
    void doSearch(const QString &qr, int maxLimit) override;
    
};

#endif // RECOLLSEARCH_H
