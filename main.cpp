#include <QApplication>
#include <QDir>
#include <QStringList>

#include "specwidgets.h"
#include "globalcontrol.h"
#include "indexersearch.h"
#include "genericfuncs.h"

#include <sys/resource.h>

CGlobalControl* gSet;
QStringList debugMessages;

int main(int argc, char *argv[])
{
    debugMessages.clear();
    qInstallMessageHandler(stdConsoleOutput);
    qRegisterMetaType<UrlHolder>("UrlHolder");
    qRegisterMetaTypeStreamOperators<UrlHolder>("UrlHolder");
    qRegisterMetaType<QBResult>("QBResult");
    qRegisterMetaType<QDir>("QDir");

    QApplication app(argc, argv);

    gSet = new CGlobalControl(&app);
    if ((gSet==NULL) || (gSet->ipcServer==NULL))
        return 0;

    if (gSet->createCoredumps) {
        // create core dumps on segfaults
        rlimit rlp;
        getrlimit(RLIMIT_CORE, &rlp);
        rlp.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlp);
    }

    app.setStyle(new QSpecMenuStyle);
    QApplication::setQuitOnLastWindowClosed(false);

    gSet->addMainWindow();

    return app.exec();
}
