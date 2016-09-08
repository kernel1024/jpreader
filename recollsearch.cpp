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
                 << "-q" << qr);
    recoll.waitForFinished(60000);
    while (!recoll.atEnd()) {
        QString s = QString::fromUtf8(recoll.readLine()).trimmed();
        QStringList sl = s.split(' ');

        if (sl.count() != 3) continue; // must be 3 elements

        QString fname = QString();
        QString title = QString();
        double rel = 0.0;

        QUrl u = QUrl(strFromBase64(sl.at(0)));
        if (u.isValid() && u.isLocalFile())
            fname = u.toLocalFile();

        title = strFromBase64(sl.at(1));

        QString p = strFromBase64(sl.at(2)).trimmed();
        p.remove('%');
        bool ok;
        int ip = p.toInt(&ok);
        if (ok)
            rel = static_cast<double>(ip)/100.0;

        emit addHitFull(fname,title,rel);
    }
    working = false;
    emit finished();
}
