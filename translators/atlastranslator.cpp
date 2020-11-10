#include <QRegularExpression>
#include <QUrl>
#include "atlastranslator.h"

CAtlasTranslator::CAtlasTranslator(QObject *parent, const QString& host, quint16 port,
                                   const CLangPair &lang) :
    CAbstractTranslator(parent, lang)
{
    m_atlHost=host;
    m_atlPort=port;
    if (gSet)
        m_emptyRestore=gSet->settings()->emptyRestore;

    QSslConfiguration conf = m_sock.sslConfiguration();
    conf.setProtocol(gSet->settings()->atlProto);
    m_sock.setSslConfiguration(conf);
    m_sock.ignoreSslErrors(gSet->ignoredSslErrorsList());

    connect(&m_sock,qOverload<const QList<QSslError>&>(&QSslSocket::sslErrors),
            this,&CAtlasTranslator::sslError);
    connect(&m_sock,&QSslSocket::errorOccurred,this,&CAtlasTranslator::socketError);
    connect(this,&CAtlasTranslator::sslCertErrors,gSet,&CGlobalControl::sslCertErrors);
}

CAtlasTranslator::~CAtlasTranslator()
{
    if (m_sock.isOpen())
        doneTran();
}

bool CAtlasTranslator::initTran()
{
    if (m_inited) return true;
    if (m_sock.isOpen()) return true;

    if (!language().isValid() || !language().isAtlasAcceptable()) {
        setErrorMsg(QSL("ATLAS: Unacceptable language pair. "
                                   "Only english and japanese is supported."));
        qCritical() << "ATLAS: Unacceptable language pair";
        return false;
    }

    if (gSet->settings()->proxyUseTranslator) {
        m_sock.setProxy(QNetworkProxy::DefaultProxy);
    } else {
        m_sock.setProxy(QNetworkProxy::NoProxy);
    }

    m_sock.connectToHostEncrypted(m_atlHost,m_atlPort);
    if (!m_sock.waitForEncrypted()) {
        setErrorMsg(QSL("ATLAS: SSL connection timeout"));
        qCritical() << "ATLAS: SSL connection timeout";
        return false;
    }
    QByteArray buf;

    // INIT command and response
    buf = QSL("INIT:%1\r\n").arg(gSet->settings()->atlToken).toLatin1();
    Q_EMIT translatorBytesTransferred(buf.size());
    m_sock.write(buf);
    m_sock.flush();
    if (!m_sock.canReadLine()) {
        if (!m_sock.waitForReadyRead()) {
            setErrorMsg(QSL("ATLAS: initialization timeout"));
            qCritical() << "ATLAS: initialization timeout";
            m_sock.close();
            return false;
        }
    }
    buf = m_sock.readLine().simplified();
    if (buf.isEmpty() || (!QString::fromLatin1(buf).startsWith(QSL("OK")))) {
        setErrorMsg(QSL("ATLAS: initialization error"));
        qCritical() << "ATLAS: initialization error";
        m_sock.close();
        return false;
    }

    // DIR command and response
    QString trandir = QSL("DIR:AUTO\r\n");
    if (language().langTo.bcp47Name().startsWith(QSL("en")))
        trandir = QSL("DIR:JE\r\n");
    if (language().langTo.bcp47Name().startsWith(QSL("ja")))
        trandir = QSL("DIR:EJ\r\n");
    buf = trandir.toLatin1();
    Q_EMIT translatorBytesTransferred(buf.size());
    m_sock.write(buf);
    m_sock.flush();
    if (!m_sock.canReadLine()) {
        if (!m_sock.waitForReadyRead()) {
            setErrorMsg(QSL("ATLAS: direction timeout error"));
            qCritical() << "ATLAS: direction timeout error";
            m_sock.close();
            return false;
        }
    }
    buf = m_sock.readLine().simplified();
    if (buf.isEmpty() || (!QString::fromLatin1(buf).startsWith(QSL("OK")))) {
        setErrorMsg(QSL("ATLAS: direction error"));
        qCritical() << "ATLAS: direction error";
        m_sock.close();
        return false;
    }
    m_inited = true;
    clearErrorMsg();
    return true;
}

QString CAtlasTranslator::tranStringPrivate(const QString &src)
{
    if (!m_sock.isOpen()) {
        setErrorMsg(QSL("ERROR: ATLAS socket not opened"));
        return QSL("ERROR:TRAN_ATLAS_SOCK_NOT_OPENED");
    }

    // TR command and response
    QString s = QString::fromLatin1(QUrl::toPercentEncoding(src," ")).trimmed();
    if (s.isEmpty()) return QString();
//    qDebug() << "TO TRAN: " << s;
    QByteArray buf = QSL("TR:%1\r\n").arg(s).toLatin1();
    Q_EMIT translatorBytesTransferred(buf.size());
    m_sock.write(buf);
    m_sock.flush();
    QByteArray sumbuf;
    while(true) {
        if (!m_sock.canReadLine()) {
            if (!m_sock.waitForReadyRead(CDefaults::translatorConnectionTimeout)) {
                qCritical() << "ATLAS: translation timeout error";
                m_sock.close();
                setErrorMsg(QSL("ERROR: ATLAS socket error"));
                return QSL("ERROR:TRAN_ATLAS_SOCKET_ERROR");
            }
        }
        buf = m_sock.readLine();
        sumbuf.append(buf);
        if (buf.endsWith("\r\n")||buf.endsWith("\r")) break;
    }
//    qDebug() << "FROM TRAN: " << sumbuf;
    sumbuf = sumbuf.simplified();
    sumbuf.replace('+',' ');
    s = QString::fromLatin1(sumbuf);
    if (sumbuf.isEmpty() || !s.contains(QRegularExpression(QSL("^RES:")))) {
        if (s.contains(QSL("NEED_RESTART"))) {
            qCritical() << "ATLAS: translation engine slipped. Please restart again.";
            m_sock.close();
            setErrorMsg(QSL("ERROR: ATLAS slipped"));
            return QSL("ERROR:ATLAS_SLIPPED");
        }

        qCritical() << "ATLAS: translation error";
        m_sock.close();
        setErrorMsg(QSL("ERROR: ATLAS translation error"));
        return QSL("ERROR:TRAN_ATLAS_TRAN_ERROR");
    }

    s = s.remove(QRegularExpression(QSL("^RES:")));
    QString res = QUrl::fromPercentEncoding(s.toLatin1());
    if (res.trimmed().isEmpty() && m_emptyRestore)
        return src;
    return res;
}

void CAtlasTranslator::doneTranPrivate(bool lazyClose)
{
    m_inited = false;

    if (!m_sock.isOpen()) return;

    if (!lazyClose) {
        // FIN command and response
        QByteArray buf = QSL("FIN\r\n").toLatin1();
        Q_EMIT translatorBytesTransferred(buf.size());
        m_sock.write(buf);
        m_sock.flush();
        if (!m_sock.canReadLine()) {
            if (!m_sock.waitForReadyRead()) {
                qCritical() << "ATLAS: finalization timeout error";
                m_sock.close();
                return;
            }
        }
        buf = m_sock.readLine().simplified();
        if (buf.isEmpty() || (!QString::fromLatin1(buf).startsWith(QSL("OK")))) {
            qCritical() << "ATLAS: finalization error";
            m_sock.close();
            return;
        }
    }

    m_sock.close();
}

bool CAtlasTranslator::isReady()
{
    return (m_inited && m_sock.isOpen());
}

CStructures::TranslationEngine CAtlasTranslator::engine()
{
    return CStructures::teAtlas;
}

void CAtlasTranslator::sslError(const QList<QSslError> & errors)
{
    QHash<QSslCertificate,QStringList> errStrHash;
    QHash<QSslCertificate,CIntList> errIntHash;

    for (const QSslError& err : qAsConst(errors)) {
        if (gSet->settings()->sslTrustedInvalidCerts.contains(err.certificate()) &&
                gSet->settings()->sslTrustedInvalidCerts.value(err.certificate()).contains(static_cast<int>(err.error()))) continue;

        qCritical() << "ATLAS SSL error: " << err.errorString();
        errStrHash[err.certificate()].append(err.errorString());
        errIntHash[err.certificate()].append(static_cast<int>(err.error()));
    }

    for (auto it = errStrHash.constBegin(), end = errStrHash.constEnd(); it != end; ++it) {
        Q_EMIT sslCertErrors(it.key(),it.value(),errIntHash.value(it.key()));
    }
}

void CAtlasTranslator::socketError(QAbstractSocket::SocketError error)
{
    qCritical() << "ATLAS socket error: " << error;
}

