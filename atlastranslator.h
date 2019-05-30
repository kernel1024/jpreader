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
    bool inited { false };
    bool emptyRestore { false };

    Q_DISABLE_COPY(CAtlasTranslator)

public:
    explicit CAtlasTranslator(QObject *parent, const QString &host, quint16 port, const CLangPair& lang);
    ~CAtlasTranslator() override;

    bool initTran() override;
    QString tranString(const QString& src) override;
    void doneTran(bool lazyClose = false) override;
    bool isReady() override;

Q_SIGNALS:
    void sslCertErrors(const QSslCertificate& cert, const QStringList& errors, const CIntList& errCodes);

public Q_SLOTS:
    void sslError(const QList<QSslError>& errors);
    void socketError(QAbstractSocket::SocketError error);

};

#endif // ATLASTRANSLATOR_H
