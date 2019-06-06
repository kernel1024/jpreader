#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QNetworkCookie>
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

QString CWebAPIAbstractTranslator::tranString(const QString& src)
{
    if (!isReady()) {
        setErrorMsg(tr("ERROR: Translator not ready"));
        return QStringLiteral("ERROR:TRAN_NOT_READY");
    }

    const int margin = 10;

    if (src.length()>=CDefaults::maxTranslationStringLength) {
        // Split by separator chars
        QRegExp rx(QStringLiteral("(\\ |\\,|\\.|\\:|\\t)")); //RegEx for ' ' or ',' or '.' or ':' or '\t'
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

bool CWebAPIAbstractTranslator::waitForReply(QNetworkReply *reply)
{
    QEventLoop eventLoop;
    QTimer timer;

    connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timer.setSingleShot(true);
    timer.start(CDefaults::translatorConnectionTimeout);

    eventLoop.exec();

    timer.stop();

    return reply->isFinished() && reply->bytesAvailable()>0;
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

void CWebAPIAbstractTranslator::deleteNAM()
{
    if (m_nam)
        m_nam->deleteLater();
    m_nam=nullptr;
}

void CWebAPIAbstractTranslator::doneTran(bool lazyClose)
{
    Q_UNUSED(lazyClose)

    clearCredentials();
    deleteNAM();
}

bool CWebAPIAbstractTranslator::isReady()
{
    return isValidCredentials() && (m_nam != nullptr);
}
