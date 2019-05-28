#include <QRegExp>
#include <QUrl>
#include "atlastranslator.h"

CAtlasTranslator::CAtlasTranslator(QObject *parent, const QString& host, quint16 port,
                                   const CLangPair &lang) :
    CAbstractTranslator(parent, lang)
{
    atlHost=host;
    atlPort=port;
    if (gSet)
        emptyRestore=gSet->settings.emptyRestore;

    QSslConfiguration conf = sock.sslConfiguration();
    conf.setProtocol(gSet->settings.atlProto);
    sock.setSslConfiguration(conf);

    QList<QSslError> expectedErrors;
    for (auto it = gSet->atlCerts.constBegin(),
         end = gSet->atlCerts.constEnd(); it != end; ++it) {
        for (auto iit = it.value().constBegin(), iend = it.value().constEnd(); iit != iend; ++iit) {
            expectedErrors << QSslError(static_cast<QSslError::SslError>(*iit),it.key());
        }
    }

    sock.ignoreSslErrors(expectedErrors);

    connect(&sock,qOverload<const QList<QSslError>&>(&QSslSocket::sslErrors),
            this,&CAtlasTranslator::sslError);
    connect(&sock,qOverload<QAbstractSocket::SocketError>(&QSslSocket::error),
            this,&CAtlasTranslator::socketError);

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

    if (!language().isValid() || !language().isAtlasAcceptable()) {
        setErrorMsg(QStringLiteral("ATLAS: Unacceptable language pair. "
                                   "Only english and japanese is supported."));
        qCritical() << "ATLAS: Unacceptable language pair";
        return false;
    }

    if (gSet->settings.proxyUseTranslator) {
        sock.setProxy(QNetworkProxy::DefaultProxy);
    } else {
        sock.setProxy(QNetworkProxy::NoProxy);
    }

    sock.connectToHostEncrypted(atlHost,atlPort);
    if (!sock.waitForEncrypted()) {
        setErrorMsg(QStringLiteral("ATLAS: SSL connection timeout"));
        qCritical() << "ATLAS: SSL connection timeout";
        return false;
    }
    QByteArray buf;

    // INIT command and response
    buf = QStringLiteral("INIT:%1\r\n").arg(gSet->settings.atlToken).toLatin1();
    sock.write(buf);
    sock.flush();
    if (!sock.canReadLine()) {
        if (!sock.waitForReadyRead()) {
            setErrorMsg(QStringLiteral("ATLAS: initialization timeout"));
            qCritical() << "ATLAS: initialization timeout";
            sock.close();
            return false;
        }
    }
    buf = sock.readLine().simplified();
    if (buf.isEmpty() || (!QString::fromLatin1(buf).startsWith(QStringLiteral("OK")))) {
        setErrorMsg(QStringLiteral("ATLAS: initialization error"));
        qCritical() << "ATLAS: initialization error";
        sock.close();
        return false;
    }

    // DIR command and response
    QString trandir = QStringLiteral("DIR:AUTO\r\n");
    if (language().langTo.bcp47Name().startsWith(QStringLiteral("en")))
        trandir = QStringLiteral("DIR:JE\r\n");
    if (language().langTo.bcp47Name().startsWith(QStringLiteral("ja")))
        trandir = QStringLiteral("DIR:EJ\r\n");
    buf = trandir.toLatin1();
    sock.write(buf);
    sock.flush();
    if (!sock.canReadLine()) {
        if (!sock.waitForReadyRead()) {
            setErrorMsg(QStringLiteral("ATLAS: direction timeout error"));
            qCritical() << "ATLAS: direction timeout error";
            sock.close();
            return false;
        }
    }
    buf = sock.readLine().simplified();
    if (buf.isEmpty() || (!QString::fromLatin1(buf).startsWith(QStringLiteral("OK")))) {
        setErrorMsg(QStringLiteral("ATLAS: direction error"));
        qCritical() << "ATLAS: direction error";
        sock.close();
        return false;
    }
    inited = true;
    clearErrorMsg();
    return true;
}

QString CAtlasTranslator::tranString(const QString &src)
{
    if (!sock.isOpen()) {
        setErrorMsg(QStringLiteral("ERROR: ATLAS socket not opened"));
        return QStringLiteral("ERROR:TRAN_ATLAS_SOCK_NOT_OPENED");
    }

    // TR command and response
    QString s = QString::fromLatin1(QUrl::toPercentEncoding(src," ")).trimmed();
    if (s.isEmpty()) return QString();
//    qDebug() << "TO TRAN: " << s;
    QByteArray buf = QStringLiteral("TR:%1\r\n").arg(s).toLatin1();
    sock.write(buf);
    sock.flush();
    QByteArray sumbuf;
    while(true) {
        if (!sock.canReadLine()) {
            if (!sock.waitForReadyRead(translatorConnectionTimeout)) {
                qCritical() << "ATLAS: translation timeout error";
                sock.close();
                setErrorMsg(QStringLiteral("ERROR: ATLAS socket error"));
                return QStringLiteral("ERROR:TRAN_ATLAS_SOCKET_ERROR");
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
    if (sumbuf.isEmpty() || !s.contains(QRegExp(QStringLiteral("^RES:")))) {
        if (s.contains(QStringLiteral("NEED_RESTART"))) {
            qCritical() << "ATLAS: translation engine slipped. Please restart again.";
            sock.close();
            setErrorMsg(QStringLiteral("ERROR: ATLAS slipped"));
            return QStringLiteral("ERROR:ATLAS_SLIPPED");
        }

        qCritical() << "ATLAS: translation error";
        sock.close();
        setErrorMsg(QStringLiteral("ERROR: ATLAS translation error"));
        return QStringLiteral("ERROR:TRAN_ATLAS_TRAN_ERROR");
    }

    s = s.remove(QRegExp(QStringLiteral("^RES:")));
    QString res = QUrl::fromPercentEncoding(s.toLatin1());
    if (res.trimmed().isEmpty() && emptyRestore)
        return src;
    return res;
}

void CAtlasTranslator::doneTran(bool lazyClose)
{
    inited = false;

    if (!sock.isOpen()) return;

    if (!lazyClose) {
        // FIN command and response
        QByteArray buf = QStringLiteral("FIN\r\n").toLatin1();
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
        if (buf.isEmpty() || (!QString::fromLatin1(buf).startsWith(QStringLiteral("OK")))) {
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
    QHash<QSslCertificate,CIntList> errIntHash;

    for (const QSslError& err : qAsConst(errors)) {
        if (gSet->atlCerts.contains(err.certificate()) &&
                gSet->atlCerts.value(err.certificate()).contains(static_cast<int>(err.error()))) continue;

        qCritical() << "ATLAS SSL error: " << err.errorString();
        errStrHash[err.certificate()].append(err.errorString());
        errIntHash[err.certificate()].append(static_cast<int>(err.error()));
    }

    for (auto it = errStrHash.constBegin(), end = errStrHash.constEnd(); it != end; ++it) {
        emit sslCertErrors(it.key(),it.value(),errIntHash.value(it.key()));
    }
}

void CAtlasTranslator::socketError(QAbstractSocket::SocketError error)
{
    qCritical() << "ATLAS socket error: " << error;
}

