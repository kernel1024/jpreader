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
    qRegisterMetaTypeStreamOperators<QStrHash>("QStrHash");
    qRegisterMetaTypeStreamOperators<QSslCertificateHash>("QSslCertificateHash");

    QApplication app(argc, argv);

    gSet = new CGlobalControl(&app);
    if ((gSet==NULL) || (gSet->ipcServer==NULL))
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

    gSet->addMainWindow();

    return app.exec();
}
