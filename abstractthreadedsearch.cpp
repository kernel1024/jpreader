#include "abstractthreadedsearch.h"

CAbstractThreadedSearch::CAbstractThreadedSearch(QObject *parent)
    : QObject(parent), working(false)
{
}
