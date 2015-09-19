#include <QUrl>

#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>
#include <QVariant>
#include <QUrlQuery>

#include "globalcontrol.h"
#include "yandextranslator.h"

CYandexTranslator::CYandexTranslator(QObject *parent, const QString &yandexKey)
    : CAbstractTranslator(parent)
{
    clientKey = yandexKey;
    nam = NULL;
}

CYandexTranslator::~CYandexTranslator()
{

}

bool CYandexTranslator::initTran()
{
    if (nam==NULL)
        nam=new QNetworkAccessManager(this);

    if (gSet->proxyUseTranslator)
        nam->setProxy(QNetworkProxy::DefaultProxy);
    else
        nam->setProxy(QNetworkProxy::NoProxy);

    srcLang = gSet->getSourceLanguageID(TE_YANDEX);
    return true;
}

QString CYandexTranslator::tranString(QString src)
{
    if (!isReady()) return QString();

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
            if (s.startsWith("ERR:")) {
                res=s;
                break;
            }
            res+=s;
        }
        return res;
    } else
        return tranStringInternal(src);
}

void CYandexTranslator::doneTran(bool lazyClose)
{
    Q_UNUSED(lazyClose);
}

bool CYandexTranslator::isReady()
{
    return !clientKey.isEmpty() && nam!=NULL;
}

bool CYandexTranslator::waitForReply(QNetworkReply *reply)
{
    QEventLoop eventLoop;
    QTimer *timer = new QTimer(this);

    connect(reply,SIGNAL(finished()), &eventLoop, SLOT(quit()));
    connect(timer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
    timer->setSingleShot(true);
    timer->start(30000);

    eventLoop.exec();

    if (timer != NULL)
    {
        timer->stop();
        timer->deleteLater();
    }

    return reply->isFinished() && reply->bytesAvailable()>0;
}

QString CYandexTranslator::tranStringInternal(QString src)
{
    QUrl rqurl = QUrl("https://translate.yandex.net/api/v1.5/tr.json/translate");

    QUrlQuery rqData;
    rqData.addQueryItem("key",clientKey);
    rqData.addQueryItem("lang",QString("%1-en").arg(srcLang));
    rqData.addQueryItem("text",QUrl::toPercentEncoding(src));

    QNetworkRequest rq(rqurl);
    rq.setHeader(QNetworkRequest::ContentTypeHeader,
                 "application/x-www-form-urlencoded");

    QNetworkReply *rpl = nam->post(rq,rqData.toString(QUrl::FullyEncoded).toUtf8());

    if (!waitForReply(rpl)) return QString("ERR:YANDEX_NETWORK_ERROR");

    QByteArray ra = rpl->readAll();

    QJsonDocument jdoc = QJsonDocument::fromJson(ra);

    if (jdoc.isNull() || jdoc.isEmpty())
        return QString("ERR:YANDEX_JSON_ERROR");

    QJsonObject jroot = jdoc.object();
    int code = jroot.value("code").toInt(500);

    if (code!=200 || !jroot.value("text").isArray()) {
        qCritical() << "Yandex error:" << jroot.value("message").toString();
        return QString("ERR:YANDEX_TRAN_ERROR");
    }

    QVariantList vl = jroot.value("text").toArray().toVariantList();
    QString res;
    res.clear();

    foreach (const QVariant& value, vl)
        res += value.toString();

    return res;
}

