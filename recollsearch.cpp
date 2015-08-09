#include <QProcess>
#include <QFileInfo>
#include <QStringList>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include "recollsearch.h"

CRecollSearch::CRecollSearch(QObject *parent) :
    CAbstractThreadedSearch(parent)
{
}

QString CRecollSearch::strFromBase64(const QString &src)
{
    QByteArray ba = src.toLatin1();
    return QString::fromUtf8(QByteArray::fromBase64(ba));
}

void CRecollSearch::doSearch(const QString &qr, int maxLimit)
{
    if (working) return;
    working = true;
    QProcess recoll;
    recoll.start("recoll",QStringList() << "-n" << QString::number(maxLimit)
                 << "-a" << "-t" << "-F" << "url caption relevancyrating"
                 << "-N" << "-q" << qr);
    recoll.waitForFinished(60000);
    while (!recoll.atEnd()) {
        QString s = QString::fromUtf8(recoll.readLine()).trimmed();
        QStringList sl = s.split(' ');

        if (sl.first()!="url") continue;
        if (sl.count()%2 != 0) continue; // elements must be in pairs by two

        QString fname = QString();
        QString title = QString();
        float rel = 0.0;

        for (int i=0;i<sl.count()/2;i++) {
            QString key = sl.at(i*2);
            QString value = strFromBase64(sl.at(i*2+1));
            if (key=="url") {
                QUrl u = QUrl(value);
                if (u.isValid() && u.isLocalFile())
                    fname = u.toLocalFile();
            } else if (key=="caption")
                title = value;
            else if (key=="relevancyrating") {
                QString p = value.trimmed();
                p.remove('%');
                bool ok;
                int ip = p.toInt(&ok);
                if (ok)
                    rel = (float)ip/100.0;
            }
        }

        emit addHitFull(fname,title,rel);
    }
    working = false;
    emit finished();
}
