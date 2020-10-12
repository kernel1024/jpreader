#include <QApplication>
#include "globalcontrol.h"

int main(int argc, char *argv[])
{
    bool cliMode = false;
    CGlobalControl::preinit(argc, argv, &cliMode);

    if (cliMode) {
        QScopedPointer<QCoreApplication> app(new QCoreApplication(argc, argv));
        gSet->initialize();
        return app->exec();
    }

    QScopedPointer<QApplication> app(new QApplication(argc, argv));
    gSet->initialize();
    return app->exec();
}
