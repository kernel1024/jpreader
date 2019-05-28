#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include "webapiabstracttranslator.h"

#define MAX_BLOCK_LENGTH 5000

CWebAPIAbstractTranslator::CWebAPIAbstractTranslator(QObject *parent, const CLangPair &lang)
    : CAbstractTranslator (parent, lang)
{
    nam = nullptr;
}

CWebAPIAbstractTranslator::~CWebAPIAbstractTranslator()
{
    deleteNAM();
}

QString CWebAPIAbstractTranslator::tranString(const QString& src)
{
    if (!isReady()) {
        setErrorMsg(tr("ERROR: Translator not ready"));
        return QStringLiteral("ERROR:TRAN_NOT_READY");
    }

    if (src.length()>=MAX_BLOCK_LENGTH) {
        // Split by separator chars
        QRegExp rx(QStringLiteral("(\\ |\\,|\\.|\\:|\\t)")); //RegEx for ' ' or ',' or '.' or ':' or '\t'
        QStringList srcl = src.split(rx);
        // Check for max length
        QStringList srout;
        srout.clear();
        for (int i=0;i<srcl.count();i++) {
            QString s = srcl.at(i);
            while (!s.isEmpty()) {
                srout << s.left(MAX_BLOCK_LENGTH-10);
                s.remove(0,MAX_BLOCK_LENGTH-10);
            }
        }
        srcl.clear();
        QString res;
        res.clear();
        for (int i=0;i<srout.count();i++) {
            QString s = tranStringInternal(srout.at(i));
            if (!getErrorMsg().isEmpty()) {
                res=s;
                break;
            }
            res+=s;
        }
        return res;
    }

    return tranStringInternal(src);
}

bool CWebAPIAbstractTranslator::waitForReply(QNetworkReply *reply)
{
    QEventLoop eventLoop;
    auto timer = new QTimer(this);

    connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    connect(timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timer->setSingleShot(true);
    timer->start(translatorConnectionTimeout);

    eventLoop.exec();

    timer->stop();
    timer->deleteLater();

    return reply->isFinished() && reply->bytesAvailable()>0;
}

void CWebAPIAbstractTranslator::initNAM()
{
    if (nam==nullptr) {
        nam=new QNetworkAccessManager(this);
        auto cj = new CNetworkCookieJar();
        nam->setCookieJar(cj);
    }

    auto cj = qobject_cast<CNetworkCookieJar *>(nam->cookieJar());
    auto mj = qobject_cast<CNetworkCookieJar *>(gSet->auxNetManager->cookieJar());
    cj->initAllCookies(mj->getAllCookies());

    if (gSet->settings.proxyUseTranslator)
        nam->setProxy(QNetworkProxy::DefaultProxy);
    else
        nam->setProxy(QNetworkProxy::NoProxy);
}

void CWebAPIAbstractTranslator::deleteNAM()
{
    if (nam)
        nam->deleteLater();
    nam=nullptr;
}

void CWebAPIAbstractTranslator::doneTran(bool)
{
    clearCredentials();
    deleteNAM();
}

bool CWebAPIAbstractTranslator::isReady()
{
    return isValidCredentials() && nam;
}
