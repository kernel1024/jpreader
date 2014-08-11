#ifndef ATLASTRANSLATOR_H
#define ATLASTRANSLATOR_H

#include <QObject>
#include <QTcpSocket>
#include <QString>

class CAtlasTranslator : public QObject
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
    bool inited;
    ATTranslateMode tranMode;
public:
    explicit CAtlasTranslator(QObject *parent = 0, QString host = QString("localhost"), int port = 18000, ATTranslateMode TranMode = AutoTran);
    virtual ~CAtlasTranslator();

    bool initTran(QString host, int port, ATTranslateMode TranMode = AutoTran);
    QString tranString(QString src);
    void doneTran(bool lazyClose = false);
    bool isReady();

signals:

public slots:

};

#endif // ATLASTRANSLATOR_H
