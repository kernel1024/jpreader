#include "abstractthreadedsearch.h"

bool CAbstractThreadedSearch::isWorking()
{
    return m_working;
}

void CAbstractThreadedSearch::setWorking(bool working)
{
    m_working = working;
}

CAbstractThreadedSearch::CAbstractThreadedSearch(QObject *parent)
    : QObject(parent)
{
}
