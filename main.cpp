#include <QApplication>
#include <QDir>
#include <QStringList>

#include "specwidgets.h"
#include "globalcontrol.h"
#include "indexersearch.h"
#include "genericfuncs.h"

#include <sys/resource.h>

#include "qtsingleapplication.h"

CGlobalControl* gSet;
QStringList debugMessages;

int main(int argc, char *argv[])
{
    debugMessages.clear();
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    qInstallMessageHandler(stdConsoleOutput);
#else
    qInstallMsgHandler(stdConsoleOutput);
#endif
    qRegisterMetaType<UrlHolder>("UrlHolder");
    qRegisterMetaTypeStreamOperators<UrlHolder>("UrlHolder");
    qRegisterMetaType<QBResult>("QBResult");
    qRegisterMetaType<QDir>("QDir");

    QtSingleApplication app(argc, argv);
    if (app.isRunning())
        return !app.sendMessage("newWindow");

    gSet = new CGlobalControl(&app);

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
