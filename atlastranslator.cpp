#include <QRegExp>
#include <QUrl>
#include "atlastranslator.h"

CAtlasTranslator::CAtlasTranslator(QObject *parent, QString host, quint16 port, const QString &SrcLang) :
    CAbstractTranslator(parent, SrcLang)
{
    atlHost=host;
    atlPort=port;
    inited=false;
    emptyRestore=false;
    if (gSet!=NULL)
        emptyRestore=gSet->settings.emptyRestore;

    QSslConfiguration conf = sock.sslConfiguration();
    conf.setProtocol(gSet->settings.atlProto);
    sock.setSslConfiguration(conf);

    QList<QSslError> expectedErrors;
    foreach (const QSslCertificate& cert, gSet->atlCerts.keys())
        foreach (const int errCode, gSet->atlCerts.value(cert))
            expectedErrors << QSslError(static_cast<QSslError::SslError>(errCode),cert);
    sock.ignoreSslErrors(expectedErrors);

    connect(&sock,SIGNAL(sslErrors(QList<QSslError>)),this,SLOT(sslError(QList<QSslError>)));
    connect(&sock,SIGNAL(error(QAbstractSocket::SocketError)),
            this,SLOT(socketError(QAbstractSocket::SocketError)));

    connect(this,&CAtlasTranslator::sslCertErrors,gSet,&CGlobalControl::atlSSLCertErrors);
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

    if (gSet->settings.proxyUseTranslator)
        sock.setProxy(QNetworkProxy::DefaultProxy);
    else
        sock.setProxy(QNetworkProxy::NoProxy);

    sock.connectToHostEncrypted(atlHost,atlPort);
    if (!sock.waitForEncrypted()) {
        tranError = QString("ATLAS: SSL connection timeout");
        qCritical() << "ATLAS: SSL connection timeout";
        return false;
    }
    QByteArray buf;

    // INIT command and response
    buf = QString("INIT:%1\r\n").arg(gSet->settings.atlToken).toLatin1();
    sock.write(buf);
    sock.flush();
    if (!sock.canReadLine()) {
        if (!sock.waitForReadyRead()) {
            tranError = QString("ATLAS: initialization timeout");
            qCritical() << "ATLAS: initialization timeout";
            sock.close();
            return false;
        }
    }
    buf = sock.readLine().simplified();
    if (buf.isEmpty() || (QString::fromLatin1(buf)!="OK")) {
        tranError = QString("ATLAS: initialization error");
        qCritical() << "ATLAS: initialization error";
        sock.close();
        return false;
    }

    // DIR command and response
    QString trandir = QString("DIR:AUTO\r\n");
    if (srcLang.startsWith("en"))
        trandir = QString("DIR:EJ\r\n");
    if (srcLang.startsWith("ja"))
        trandir = QString("DIR:JE\r\n");
    buf = trandir.toLatin1();
    sock.write(buf);
    sock.flush();
    if (!sock.canReadLine()) {
        if (!sock.waitForReadyRead()) {
            tranError = QString("ATLAS: direction timeout error");
            qCritical() << "ATLAS: direction timeout error";
            sock.close();
            return false;
        }
    }
    buf = sock.readLine().simplified();
    if (buf.isEmpty() || (QString::fromLatin1(buf)!="OK")) {
        tranError = QString("ATLAS: direction error");
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

void CAtlasTranslator::sslError(const QList<QSslError> & errors)
{
    QHash<QSslCertificate,QStringList> errStrHash;
    QHash<QSslCertificate,QIntList> errIntHash;

    foreach (const QSslError& err, errors) {
        if (gSet->atlCerts.contains(err.certificate()) &&
                gSet->atlCerts.value(err.certificate()).contains(static_cast<int>(err.error()))) continue;

        qCritical() << "ATLAS SSL error: " + err.errorString();
        errStrHash[err.certificate()].append(err.errorString());
        errIntHash[err.certificate()].append(static_cast<int>(err.error()));
    }

    foreach (const QSslCertificate& key, errStrHash.keys()) {
        emit sslCertErrors(key,errStrHash.value(key),errIntHash.value(key));
    }
}

void CAtlasTranslator::socketError(const QAbstractSocket::SocketError error)
{
    qCritical() << "ATLAS socket error: " << error;
}

