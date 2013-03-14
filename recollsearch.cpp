#include <QProcess>
#include <QFileInfo>
#include <QStringList>
#include "recollsearch.h"

CRecollSearch::CRecollSearch(QObject *parent) :
    QObject(parent)
{
    working = false;
}


void CRecollSearch::doSearch(const QString &qr, int maxLimit)
{
    if (working) return;
    working = true;
    QProcess recoll;
    recoll.start("recoll",QStringList() << "-n" << QString::number(maxLimit)
                 << "-b" << "-a" << "-t" << "-q" << qr);
    recoll.waitForFinished(60000);
    while (!recoll.atEnd()) {
        QString fname = QString::fromUtf8(recoll.readLine()).trimmed();
        if (fname.endsWith('/') || fname.endsWith('\\')) continue;
        fname.remove("file://",Qt::CaseInsensitive);
        emit addHit(fname);
    }
    working = false;
    emit finished();
}
