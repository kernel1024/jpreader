#ifndef ATLASTRANSLATOR_H
#define ATLASTRANSLATOR_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <abstracttranslator.h>

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
    QTcpSocket sock;
    QString atlHost;
    int atlPort;
    ATTranslateMode tranMode;
    bool inited;
    bool emptyRestore;
public:
    explicit CAtlasTranslator(QObject *parent, QString host, int port, ATTranslateMode TranMode = AutoTran);
    virtual ~CAtlasTranslator();

    bool initTran();
    QString tranString(QString src);
    void doneTran(bool lazyClose = false);
    bool isReady();

signals:

public slots:

};

#endif // ATLASTRANSLATOR_H
