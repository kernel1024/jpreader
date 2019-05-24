#include "abstractthreadedsearch.h"

bool CAbstractThreadedSearch::isWorking() const
{
    return m_working;
}

void CAbstractThreadedSearch::setWorking(bool value)
{
    m_working = value;
}

CAbstractThreadedSearch::CAbstractThreadedSearch(QObject *parent)
    : QObject(parent)
{
}
