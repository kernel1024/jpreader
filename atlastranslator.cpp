#include <QRegExp>
#include <QUrl>
#include "atlastranslator.h"
#include "globalcontrol.h"

CAtlasTranslator::CAtlasTranslator(QObject *parent, QString host, int port, ATTranslateMode TranMode) :
    CAbstractTranslator(parent)
{
    atlHost=host;
    atlPort=port;
    inited=false;
    tranMode=TranMode;
    emptyRestore=false;
    if (gSet!=NULL)
        emptyRestore=gSet->emptyRestore;
}

CAtlasTranslator::~CAtlasTranslator()
{
    if (sock.isOpen())
        doneTran();
}

bool CAtlasTranslator::initTran()
{
    if (inited) return true;
    if (sock.isOpen()) return true;

    if (gSet->proxyUseTranslator)
        sock.setProxy(QNetworkProxy::DefaultProxy);
    else
        sock.setProxy(QNetworkProxy::NoProxy);

    sock.connectToHost(atlHost,atlPort);
    if (!sock.waitForConnected()) {
        qCritical() << "ATLAS: connection timeout";
        return false;
    }
    QByteArray buf;

    // INIT command and response
    buf = QString("INIT\r\n").toLatin1();
    sock.write(buf);
    sock.flush();
    if (!sock.canReadLine()) {
        if (!sock.waitForReadyRead()) {
            qCritical() << "ATLAS: initialization timeout";
            sock.close();
            return false;
        }
    }
    buf = sock.readLine().simplified();
    if (buf.isEmpty() || (QString::fromLatin1(buf)!="OK")) {
        qCritical() << "ATLAS: initialization error";
        sock.close();
        return false;
    }

    // DIR command and response
    QString trandir = QString("DIR:AUTO\r\n");
    if (tranMode == EngToJpnTran)
        trandir = QString("DIR:EJ\r\n");
    if (tranMode == JpnToEngTran)
        trandir = QString("DIR:JE\r\n");
    buf = trandir.toLatin1();
    sock.write(buf);
    sock.flush();
    if (!sock.canReadLine()) {
        if (!sock.waitForReadyRead()) {
            qCritical() << "ATLAS: direction timeout error";
            sock.close();
            return false;
        }
    }
    buf = sock.readLine().simplified();
    if (buf.isEmpty() || (QString::fromLatin1(buf)!="OK")) {
        qCritical() << "ATLAS: direction error";
        sock.close();
        return false;
    }
    inited = true;
    tranError.clear();
    return true;
}

QString CAtlasTranslator::tranString(QString src)
{
    if (!sock.isOpen()) {
        tranError = QString("ERROR: ATLAS socket not opened");
        return "ERROR:TRAN_ATLAS_SOCK_NOT_OPENED";
    }

    // TR command and response
    QString s = QString::fromLatin1(QUrl::toPercentEncoding(src," ")).trimmed();
    if (s.isEmpty()) return "";
//    qDebug() << "TO TRAN: " << s;
    QByteArray buf = QString("TR:%1\r\n").arg(s).toLatin1();
    sock.write(buf);
    sock.flush();
    QByteArray sumbuf;
    while(true) {
        if (!sock.canReadLine()) {
            if (!sock.waitForReadyRead(60000)) {
                qCritical() << "ATLAS: translation timeout error";
                sock.close();
                tranError = QString("ERROR: ATLAS socket error");
                return "ERROR:TRAN_ATLAS_SOCKET_ERROR";
            }
        }
        buf = sock.readLine();
        sumbuf.append(buf);
        if (buf.endsWith("\r\n")||buf.endsWith("\r")) break;
    }
//    qDebug() << "FROM TRAN: " << sumbuf;
    sumbuf = sumbuf.simplified();
    sumbuf.replace('+',' ');
    s = QString::fromLatin1(sumbuf);
    if (sumbuf.isEmpty() || !s.contains(QRegExp("^RES:"))) {
        if (s.contains("NEED_RESTART")) {
            qCritical() << "ATLAS: translation engine slipped. Please restart again.";
            sock.close();
            tranError = QString("ERROR: ATLAS slipped");
            return "ERROR:ATLAS_SLIPPED";
        } else {
            qCritical() << "ATLAS: translation error";
            sock.close();
            tranError = QString("ERROR: ATLAS translation error");
            return "ERROR:TRAN_ATLAS_TRAN_ERROR";
        }
    }

    s = s.remove(QRegExp("^RES:"));
    QString res = QUrl::fromPercentEncoding(s.toLatin1());
    if (res.trimmed().isEmpty() && emptyRestore)
        return src;
    return res;
}

void CAtlasTranslator::doneTran(bool lazyClose)
{
    if (!sock.isOpen()) return;

    if (!lazyClose) {
        // FIN command and response
        QByteArray buf = QString("FIN\r\n").toLatin1();
        sock.write(buf);
        sock.flush();
        if (!sock.canReadLine()) {
            if (!sock.waitForReadyRead()) {
                qCritical() << "ATLAS: finalization timeout error";
                sock.close();
                return;
            }
        }
        buf = sock.readLine().simplified();
        if (buf.isEmpty() || (QString::fromLatin1(buf)!="OK")) {
            qCritical() << "ATLAS: finalization error";
            sock.close();
            return;
        }
    }
    sock.close();
}

bool CAtlasTranslator::isReady()
{
    return (inited && sock.isOpen());
}

