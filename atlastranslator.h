#ifndef ATLASTRANSLATOR_H
#define ATLASTRANSLATOR_H

#include <QObject>
#include <QSslSocket>
#include <QString>
#include <abstracttranslator.h>
#include "globalcontrol.h"

class CAtlasTranslator : public CAbstractTranslator
{
    Q_OBJECT
public:
    enum ATTranslateMode {
        AutoTran,
        EngToJpnTran,
        JpnToEngTran
    };
private:
    QSslSocket sock;
    QString atlHost;
    quint16 atlPort;
    bool inited;
    bool emptyRestore;
public:
    explicit CAtlasTranslator(QObject *parent, QString host, quint16 port, const CLangPair& lang);
    virtual ~CAtlasTranslator();

    bool initTran();
    QString tranString(const QString& src);
    void doneTran(bool lazyClose = false);
    bool isReady();

signals:
    void sslCertErrors(const QSslCertificate& cert, const QStringList& errors, const QIntList& errCodes);

public slots:
    void sslError(const QList<QSslError>&);
    void socketError(const QAbstractSocket::SocketError error);

};

#endif // ATLASTRANSLATOR_H
