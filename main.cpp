#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QSslCertificate>

#include "specwidgets.h"
#include "globalcontrol.h"
#include "indexersearch.h"
#include "genericfuncs.h"
#include "adblockrule.h"

#include <sys/resource.h>

CGlobalControl* gSet = NULL;
QStringList debugMessages;

int main(int argc, char *argv[])
{
    int dbgport = getRandomTCPPort();
    if (dbgport>0) {
        QString url = QString("127.0.0.1:%1").arg(dbgport);
        setenv("QTWEBENGINE_REMOTE_DEBUGGING",url.toUtf8().constData(),1);
    }
    debugMessages.clear();
    qInstallMessageHandler(stdConsoleOutput);
    qRegisterMetaType<UrlHolder>("UrlHolder");
    qRegisterMetaType<QBResult>("QBResult");
    qRegisterMetaType<QDir>("QDir");
    qRegisterMetaType<QSslCertificate>("QSslCertificate");
    qRegisterMetaType<QIntList>("QIntList");
    qRegisterMetaTypeStreamOperators<UrlHolder>("UrlHolder");
    qRegisterMetaTypeStreamOperators<CAdBlockRule>("CAdBlockRule");
    qRegisterMetaTypeStreamOperators<CAdBlockList>("CAdBlockList");
    qRegisterMetaTypeStreamOperators<QUHList>("QUHList");
    qRegisterMetaTypeStreamOperators<QBookmarksMap>("QBookmarksMap");
    qRegisterMetaTypeStreamOperators<QBookmarks>("QBookmarks");
    qRegisterMetaTypeStreamOperators<QStrHash>("QStrHash");
    qRegisterMetaTypeStreamOperators<QSslCertificateHash>("QSslCertificateHash");

    QApplication app(argc, argv);

    gSet = new CGlobalControl(&app);
    gSet->inspectorPort = dbgport;
    if (gSet->ipcServer==NULL)
        return 0;

    if (gSet->settings.createCoredumps) {
        // create core dumps on segfaults
        rlimit rlp;
        getrlimit(RLIMIT_CORE, &rlp);
        rlp.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlp);
    }

    app.setStyle(new CSpecMenuStyle);
    QApplication::setQuitOnLastWindowClosed(false);

    gSet->ui.addMainWindow();

    return app.exec();
}
