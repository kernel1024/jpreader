#include <QApplication>
#include "global/globalcontrol.h"
#include "global/startup.h"

int main(int argc, char *argv[])
{
    bool cliMode = false;
    CGlobalStartup::preinit(argc, argv, &cliMode);

    if (cliMode) {
        QScopedPointer<QCoreApplication> app(new QCoreApplication(argc, argv));
        gSet->startup()->initialize();
        return app->exec();
    }

    QScopedPointer<QApplication> app(new QApplication(argc, argv));
    gSet->startup()->initialize();
    return app->exec();
}
