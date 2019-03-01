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

void CRecollSearch::recollReadyRead()
{
    auto recoll = qobject_cast<QProcess *>(sender());
    if (recoll==nullptr) return;

    QStringList outList= QString::fromUtf8(recoll->readAllStandardOutput()).split('\n');

    for(int i=0;i<outList.count();i++) {

        QString s = outList.at(i).trimmed();
        QStringList sl = s.split(' ');

        if (sl.count() != 4) continue; // must be 4 elements

        QString fname = QString();
        double rel = 0.0;

        QUrl u = QUrl(strFromBase64(sl.at(0)));
        if (u.isValid() && u.isLocalFile())
            fname = u.toLocalFile();

        QString title = strFromBase64(sl.at(1));

        QString p = strFromBase64(sl.at(2)).trimmed();
        p.remove('%');
        bool ok;
        int ip = p.toInt(&ok);
        if (ok)
            rel = static_cast<double>(ip)/100.0;

        QString snippet = strFromBase64(sl.at(3)).trimmed();

        emit addHit(fname,title,rel,snippet);
    }
}

void CRecollSearch::recollFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    working = false;
    emit finished();
}

void CRecollSearch::doSearch(const QString &qr, int maxLimit)
{
    if (working) return;
    working = true;

    auto recoll = new QProcess(this);
    recoll->setEnvironment(QProcess::systemEnvironment());
    recoll->setProcessChannelMode(QProcess::MergedChannels);

    connect(recoll,&QProcess::readyReadStandardOutput,
            this,&CRecollSearch::recollReadyRead);

    connect(recoll,QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,&CRecollSearch::recollFinished);

    recoll->start(QStringLiteral("recoll"),QStringList() << QStringLiteral("-n")
                  << QString::number(maxLimit) << QStringLiteral("-t")
                  << QStringLiteral("-F")
                  << QStringLiteral("url caption relevancyrating abstract")
                  << QStringLiteral("-q") << qr);

    recoll->waitForStarted();
}
