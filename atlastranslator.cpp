#include "atlastranslator.h"

CAtlasTranslator::CAtlasTranslator(QObject *parent, QString host, int port, ATTranslateMode TranMode) :
    QObject(parent)
{
    atlHost=host;
    atlPort=port;
    inited=false;
    tranMode=TranMode;
}

CAtlasTranslator::~CAtlasTranslator()
{
    if (sock.isOpen())
        doneTran();
}

bool CAtlasTranslator::initTran(QString host, int port, ATTranslateMode TranMode)
{
    if (inited) return true;
    if (sock.isOpen()) return true;
    atlHost=host;
    atlPort=port;
    tranMode=TranMode;
    sock.connectToHost(atlHost,atlPort);
    if (!sock.waitForConnected()) {
        qDebug() << "ATLAS: connection timeout";
        return false;
    }
    QByteArray buf;

    // INIT command and response
    buf = QString("INIT\r\n").toAscii();
    sock.write(buf);
    sock.flush();
    if (!sock.canReadLine()) {
        if (!sock.waitForReadyRead()) {
            qDebug() << "ATLAS: initialization timeout";
            sock.close();
            return false;
        }
    }
    buf = sock.readLine().simplified();
    if (buf.isEmpty() || (QString::fromAscii(buf)!="OK")) {
        qDebug() << "ATLAS: initialization error";
        sock.close();
        return false;
    }

    // DIR command and response
    QString trandir = QString("DIR:AUTO\r\n");
    if (tranMode == EngToJpnTran)
        trandir = QString("DIR:EJ\r\n");
    if (tranMode == JpnToEngTran)
        trandir = QString("DIR:JE\r\n");
    buf = trandir.toAscii();
    sock.write(buf);
    sock.flush();
    if (!sock.canReadLine()) {
        if (!sock.waitForReadyRead()) {
            qDebug() << "ATLAS: direction timeout error";
            sock.close();
            return false;
        }
    }
    buf = sock.readLine().simplified();
    if (buf.isEmpty() || (QString::fromAscii(buf)!="OK")) {
        qDebug() << "ATLAS: direction error";
        sock.close();
        return false;
    }
    return true;
}

QString CAtlasTranslator::tranString(QString src)
{
    if (!sock.isOpen()) return "ERROR: Socket not opened";

    // TR command and response
    QString s = QString::fromAscii(QUrl::toPercentEncoding(src," ")).trimmed();
    if (s.isEmpty()) return "";
//    qDebug() << "TO TRAN: " << s;
    QByteArray buf = QString("TR:%1\r\n").arg(s).toAscii();
    sock.write(buf);
    sock.flush();
    QByteArray sumbuf;
    while(true) {
        if (!sock.canReadLine()) {
            if (!sock.waitForReadyRead(60000)) {
                qDebug() << "ATLAS: translation timeout error";
                sock.close();
                return "ERROR";
            }
        }
        buf = sock.readLine();
        sumbuf.append(buf);
        if (buf.endsWith("\r\n")||buf.endsWith("\r")) break;
    }
//    qDebug() << "FROM TRAN: " << sumbuf;
    sumbuf = sumbuf.simplified();
    sumbuf.replace('+',' ');
    s = QString::fromAscii(sumbuf);
    if (sumbuf.isEmpty() || !s.contains(QRegExp("^RES:"))) {
        if (s.contains("NEED_RESTART")) {
            qDebug() << "ATLAS: translation engine slipped. Please restart again.";
            sock.close();
            return "ERROR:ATLAS_SLIPPED";
        } else {
            qDebug() << "ATLAS: translation error";
            sock.close();
            return "ERROR";
        }
    }

    s = s.remove(QRegExp("^RES:"));
    return QUrl::fromPercentEncoding(s.toAscii());
}

void CAtlasTranslator::doneTran()
{
    if (!sock.isOpen()) return;

    // FIN command and response
    QByteArray buf = QString("FIN\r\n").toAscii();
    sock.write(buf);
    sock.flush();
    if (!sock.canReadLine()) {
        if (!sock.waitForReadyRead()) {
            qDebug() << "ATLAS: finalization timeout error";
            sock.close();
            return;
        }
    }
    buf = sock.readLine().simplified();
    if (buf.isEmpty() || (QString::fromAscii(buf)!="OK")) {
        qDebug() << "ATLAS: finalization error";
        sock.close();
        return;
    }
    sock.close();
}

