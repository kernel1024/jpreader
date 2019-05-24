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
    QRegExp rxname("[a-z]+");

    for(int i=0;i<outList.count();i++) {
        QString s = outList.at(i).trimmed();
        QStringList sl = s.split(' ');
        CStringHash snip;

        if ((sl.count() % 2) != 0) continue; // must be even elements
        if (!rxname.exactMatch(sl.first())) continue; // must be started with field name

        int idx = 0;
        while (idx<sl.count()) {
            QString name = sl.at(idx++).trimmed();
            snip[name] = strFromBase64(sl.at(idx++)).trimmed();
        }

        QUrl u = QUrl(snip.value(QStringLiteral("url")));
        if (u.isValid() && u.isLocalFile())
            snip[QStringLiteral("jp:fullfilename")] = u.toLocalFile();

        emit addHit(snip);
    }
}

void CRecollSearch::recollFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    setWorking(false);
    emit finished();
}

void CRecollSearch::doSearch(const QString &qr, int maxLimit)
{
    if (isWorking()) return;
    setWorking(true);

    auto recoll = new QProcess(this);
    recoll->setEnvironment(QProcess::systemEnvironment());
    recoll->setProcessChannelMode(QProcess::MergedChannels);

    connect(recoll,&QProcess::readyReadStandardOutput,
            this,&CRecollSearch::recollReadyRead);

    connect(recoll,qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,&CRecollSearch::recollFinished);

    recoll->start(QStringLiteral("recoll"),QStringList() << QStringLiteral("-n")
                  << QString::number(maxLimit) << QStringLiteral("-t")
                  << QStringLiteral("-F") << QString() << QStringLiteral("-N")
                  << QStringLiteral("-q") << qr);

    recoll->waitForStarted();
}
