#include <QApplication>
#include "globalcontrol.h"

int main(int argc, char *argv[])
{
    CGlobalControl::preinit();
    QApplication app(argc, argv);
    gSet->initialize();
    return app.exec();
}
