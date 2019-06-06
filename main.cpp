#include <QApplication>
#include "globalcontrol.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    gSet->initialize();
    return app.exec();
}
