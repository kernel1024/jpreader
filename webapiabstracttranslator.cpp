#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QNetworkCookie>
#include <QRegularExpression>
#include "webapiabstracttranslator.h"

namespace CDefaults {
const int maxTranslationStringLength = 5000;
}

CWebAPIAbstractTranslator::CWebAPIAbstractTranslator(QObject *parent, const CLangPair &lang)
    : CAbstractTranslator (parent, lang)
{
}

CWebAPIAbstractTranslator::~CWebAPIAbstractTranslator()
{
    deleteNAM();
}

QString CWebAPIAbstractTranslator::tranStringPrivate(const QString& src)
{
    if (!isReady()) {
        setErrorMsg(tr("ERROR: Translator not ready"));
        return QSL("ERROR:TRAN_NOT_READY");
    }

    const int margin = 10;

    if (src.length()>=CDefaults::maxTranslationStringLength) {
        // Split by separator chars
        QRegularExpression rx(QSL("(\\ |\\,|\\.|\\:|\\t)")); //RegEx for ' ' or ',' or '.' or ':' or '\t'
        QStringList srcl = src.split(rx);
        // Check for max length
        QStringList srout;
        srout.clear();
        for (int i=0;i<srcl.count();i++) {
            QString s = srcl.at(i);
            while (!s.isEmpty()) {
                srout << s.left(CDefaults::maxTranslationStringLength - margin);
                s.remove(0,CDefaults::maxTranslationStringLength - margin);
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

bool CWebAPIAbstractTranslator::waitForReply(QNetworkReply *reply, int *httpStatus)
{
    QEventLoop eventLoop;
    QTimer timer;

    connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timer.setSingleShot(true);
    timer.start(CDefaults::translatorConnectionTimeout);

    eventLoop.exec();

    timer.stop();

    QVariant vstatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    bool statusOk;
    *httpStatus = vstatus.toInt(&statusOk);
    if (!statusOk)
        *httpStatus = CDefaults::httpCodeClientUnknownError;

    bool queryOk = reply->isFinished() && (reply->bytesAvailable()>0) && (reply->error()==QNetworkReply::NoError);

    if (!queryOk) {
        qCritical() << "WebAPI query failed: " << reply->url();
        qCritical() << " --- Error: " << reply->error() << ", " << reply->errorString();

        if (statusOk)
            qCritical() << " --- HTTP status code : " << (*httpStatus);
    }

    return queryOk;
}

void CWebAPIAbstractTranslator::initNAM()
{
    if (m_nam==nullptr) {
        m_nam=new QNetworkAccessManager(this);
        auto cj = new CNetworkCookieJar(m_nam);
        m_nam->setCookieJar(cj);
    }

    auto cj = qobject_cast<CNetworkCookieJar *>(m_nam->cookieJar());
    auto mj = qobject_cast<CNetworkCookieJar *>(gSet->auxNetworkAccessManager()->cookieJar());
    cj->initAllCookies(mj->getAllCookies());

    if (gSet->settings()->proxyUseTranslator) {
        m_nam->setProxy(QNetworkProxy::DefaultProxy);
    } else {
        m_nam->setProxy(QNetworkProxy::NoProxy);
    }
}

QByteArray CWebAPIAbstractTranslator::processRequest(const std::function<QNetworkRequest()> &requestFunc,
                                                     const QByteArray& body,
                                                     int *httpStatus,
                                                     bool *aborted)
{
    const QString clName = QString::fromLatin1(metaObject()->className());
    int retries = 0;
    *httpStatus = CDefaults::httpCodeClientUnknownError;
    QByteArray replyBody;
    while (retries < getTranslatorRetryCount() && !isAborted()) {
        QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(nam()->post(requestFunc(),body));

        bool replyOk = waitForReply(rpl.data(),httpStatus);

        if (!replyOk) {
            qWarning() << QSL("%1 translator network error").arg(clName);
        }

        if (*httpStatus == CDefaults::httpCodeTooManyRequests) {
            qWarning() << QSL("%1 translator throttling, delay and retry #%2.")
                          .arg(clName)
                          .arg(retries);

        } else if (*httpStatus == CDefaults::httpCodeClientUnknownError) {
            qWarning() << QSL("%1 translator no HTTP status, delay and retry #%2.")
                          .arg(clName)
                          .arg(retries);

        } else if (*httpStatus >= CDefaults::httpCodeClientError) {
            qWarning() << QSL("%1 translator bad request or credentials, aborting. HTTP status: %2.")
                          .arg(clName)
                          .arg(*httpStatus);
            break;

        } else if (replyOk) {
            replyBody = rpl->readAll();
            break;
        }

        CGenericFuncs::processedSleep(getRandomDelay());
        retries++;
    }

    *aborted = isAborted();

    return replyBody;
}

void CWebAPIAbstractTranslator::deleteNAM()
{
    if (m_nam)
        m_nam->deleteLater();
    m_nam=nullptr;
}

void CWebAPIAbstractTranslator::doneTranPrivate(bool lazyClose)
{
    Q_UNUSED(lazyClose)

    clearCredentials();
    deleteNAM();
}

bool CWebAPIAbstractTranslator::isReady()
{
    return isValidCredentials() && (m_nam != nullptr);
}
