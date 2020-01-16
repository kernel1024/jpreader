#ifndef ATLASTRANSLATOR_H
#define ATLASTRANSLATOR_H

#include <QObject>
#include <QSslSocket>
#include <QString>
#include <abstracttranslator.h>
#include "globalcontrol.h"

namespace CDefaults {
const unsigned int atlasMinRetryDelay = 3;
const unsigned int atlasMaxRetryDelay = 5;
}

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
    QSslSocket m_sock;
    QString m_atlHost;
    quint16 m_atlPort;
    bool m_inited { false };
    bool m_emptyRestore { false };

    Q_DISABLE_COPY(CAtlasTranslator)

public:
    explicit CAtlasTranslator(QObject *parent, const QString &host, quint16 port, const CLangPair& lang);
    ~CAtlasTranslator() override;

    bool initTran() override;
    QString tranStringPrivate(const QString& src) override;
    void doneTranPrivate(bool lazyClose) override;
    bool isReady() override;
    CStructures::TranslationEngine engine() override;

Q_SIGNALS:
    void sslCertErrors(const QSslCertificate& cert, const QStringList& errors, const CIntList& errCodes);

public Q_SLOTS:
    void sslError(const QList<QSslError>& errors);
    void socketError(QAbstractSocket::SocketError error);

};

#endif // ATLASTRANSLATOR_H
