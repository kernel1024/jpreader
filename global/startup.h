#ifndef CGLOBALSTARTUP_H
#define CGLOBALSTARTUP_H

#include <QObject>
#include <QLocalSocket>
#include "abstractthreadworker.h"

class CGlobalControl;

class CGlobalStartup : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CGlobalStartup)
public:
    explicit CGlobalStartup(CGlobalControl *parent);
    ~CGlobalStartup() override;

    void initialize();
    static void preinit(int &argc, char *argv[], bool *cliMode); // NOLINT

    // Worker control
    bool setupThreadedWorker(CAbstractThreadWorker *worker);
    bool isThreadedWorkersActive() const;
    void startupXapianIndexerDirect(bool fromInotify,
                                    bool cleanupDatabase);

private:
    CGlobalControl *m_g;

    bool setupIPC();
    void sendIPCMessage(QLocalSocket *socket, const QString& msg);
    void cleanTmpFiles();
    void stopAndCloseWorkers();
    void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    void initLanguagesList();

Q_SIGNALS:
    void stopWorkers();
    void terminateWorkers();
    void stopXapianWorkers();

public Q_SLOTS:
    void cleanupAndExit();
    void ipcMessageReceived();
    void cleanupWorker();
    void startupXapianIndexer();

};

#endif // CGLOBALSTARTUP_H
