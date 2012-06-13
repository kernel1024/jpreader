#include <QApplication>

#include "specwidgets.h"
#include "globalcontrol.h"

#include <sys/resource.h>

CGlobalControl* gSet;

int main(int argc, char *argv[])
{
    // create core dumps on segfaults
    rlimit rlp;
    int res = getrlimit(RLIMIT_CORE, &rlp);
    rlp.rlim_cur = RLIM_INFINITY;
    res = setrlimit(RLIMIT_CORE, &rlp);
    if (res < 0)
    {
        qDebug() << "err: setrlimit: RLIMIT_CORE";
        return -2;
    }

    qRegisterMetaType<UrlHolder>("UrlHolder");
    qRegisterMetaTypeStreamOperators<UrlHolder>("UrlHolder");


	QApplication app(argc, argv);
    gSet = new CGlobalControl(&app);
    app.setStyle(new QSpecMenuStyle);

    gSet->addMainWindow();

    return app.exec();
}
