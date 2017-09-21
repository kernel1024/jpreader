#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include "webapiabstracttranslator.h"

CWebAPIAbstractTranslator::CWebAPIAbstractTranslator(QObject *parent, const QString &SrcLang)
    : CAbstractTranslator (parent, SrcLang)
{
    nam = NULL;
}

CWebAPIAbstractTranslator::~CWebAPIAbstractTranslator()
{
    deleteNAM();
}

QString CWebAPIAbstractTranslator::tranString(const QString& src)
{
    if (!isReady()) {
        tranError = QString("ERROR: Translator not ready");
        return QString("ERROR:TRAN_NOT_READY");
    }

    if (src.length()>=10000) {
        // Split by separator chars
        QRegExp rx("(\\ |\\,|\\.|\\:|\\t)"); //RegEx for ' ' or ',' or '.' or ':' or '\t'
        QStringList srcl = src.split(rx);
        // Check for max length
        QStringList srout;
        srout.clear();
        for (int i=0;i<srcl.count();i++) {
            QString s = srcl.at(i);
            while (!s.isEmpty()) {
                srout << s.left(9990);
                s.remove(0,9990);
            }
        }
        srcl.clear();
        QString res;
        res.clear();
        for (int i=0;i<srout.count();i++) {
            QString s = tranStringInternal(srout.at(i));
            if (!tranError.isEmpty()) {
                res=s;
                break;
            }
            res+=s;
        }
        return res;
    } else
        return tranStringInternal(src);
}

bool CWebAPIAbstractTranslator::waitForReply(QNetworkReply *reply)
{
    QEventLoop eventLoop;
    QTimer *timer = new QTimer(this);

    connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    connect(timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timer->setSingleShot(true);
    timer->start(30000);

    eventLoop.exec();

    timer->stop();
    timer->deleteLater();

    return reply->isFinished() && reply->bytesAvailable()>0;
}

void CWebAPIAbstractTranslator::deleteNAM()
{
    if (nam!=NULL)
        nam->deleteLater();
    nam=NULL;
}

void CWebAPIAbstractTranslator::doneTran(bool)
{
    clearCredentials();
    deleteNAM();
}

bool CWebAPIAbstractTranslator::isReady()
{
    return isValidCredentials() && nam!=NULL;
}
