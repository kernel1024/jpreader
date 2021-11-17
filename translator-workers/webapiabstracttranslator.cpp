#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QNetworkCookie>
#include <QJsonDocument>
#include <QJsonObject>

#include "webapiabstracttranslator.h"
#include "global/control.h"
#include "global/network.h"
#include "utils/genericfuncs.h"
#include "utils/specwidgets.h"

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

    QString res;
    for (const QString &str : splitLongText(src)) {
        QString s = tranStringInternal(str);
        if (!getErrorMsg().isEmpty()) {
            res=s;
            break;
        }
        res.append(s);
    }
    return res;
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

    *httpStatus = CGenericFuncs::getHttpStatusFromReply(reply);

    bool queryOk = reply->isFinished() && (reply->bytesAvailable()>0) && (reply->error()==QNetworkReply::NoError);

    if (!queryOk) {
        qCritical() << "WebAPI query failed: " << reply->url();
        qCritical() << " --- Error: " << reply->error() << ", " << reply->errorString();
        qCritical() << " --- HTTP status code : " << (*httpStatus);
    }

    return queryOk;
}

void CWebAPIAbstractTranslator::initNAM()
{
    if (m_nam==nullptr) {
        m_nam=new QNetworkAccessManager(this);
        auto *cj = new CNetworkCookieJar(m_nam);
        m_nam->setCookieJar(cj);
    }

    if (gSet->actions()) {
        auto *cj = qobject_cast<CNetworkCookieJar *>(m_nam->cookieJar());
        auto *mj = qobject_cast<CNetworkCookieJar *>(gSet->auxNetworkAccessManager()->cookieJar());
        cj->initAllCookies(mj->getAllCookies());
    }

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
    int delayFrac = 1;
    *httpStatus = CDefaults::httpCodeClientUnknownError;
    QByteArray replyBody;
    while (retries < getTranslatorRetryCount() && !isAborted()) {
        QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(nam()->post(requestFunc(),body));
        Q_EMIT translatorBytesTransferred(body.size());

        bool replyOk = waitForReply(rpl.data(),httpStatus);
        replyBody = rpl->readAll();

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

        } else if (*httpStatus == CDefaults::httpCodeClientError && rpl->url().toString().contains(QSL("aliyun"))) {
            // signature error from AliCloud, try again
            QJsonDocument doc;
            bool noRetry = true;
            if (!replyBody.isEmpty()) {
                doc = QJsonDocument::fromJson(replyBody);
                if (!doc.isEmpty()) {
                    if (doc.object().value(QSL("Code")).toString() == QSL("SignatureDoesNotMatch")) {
                        qWarning() << QSL("%1 translator Alibaba Cloud signature auth error, delay and retry #%2.")
                                      .arg(clName)
                                      .arg(retries);
                        noRetry = false;
                        delayFrac = CDefaults::tranAliDelayFrac;
                    }
                }
            }
            if (noRetry) {
                qWarning() << QSL("%1 translator bad request or credentials, aborting. HTTP status: %2.")
                              .arg(clName)
                              .arg(*httpStatus);
                break;
            }

        } else if (*httpStatus >= CDefaults::httpCodeClientError) {
            qWarning() << QSL("%1 translator bad request or credentials, aborting. HTTP status: %2.")
                          .arg(clName)
                          .arg(*httpStatus);
            break;

        } else if (replyOk) {
            break;
        }

        CGenericFuncs::processedMSleep(getRandomDelay(CDefaults::tranMinRetryDelay/delayFrac,
                                                      CDefaults::tranMaxRetryDelay/delayFrac));
        retries++;
    }

    *aborted = isAborted();

    return replyBody;
}

void CWebAPIAbstractTranslator::clearCredentials()
{
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
