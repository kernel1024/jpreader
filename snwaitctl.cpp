#include "snwaitctl.h"

CSnWaitCtl::CSnWaitCtl(CSnippetViewer *parent) :
    QObject(parent)
{
    snv = parent;
    setText(QString());
    setProgressValue(0);
    setProgressEnabled(true);
}

void CSnWaitCtl::setText(const QString& aMsg)
{
    snv->lblWaitMessage->setText(aMsg);
}

void CSnWaitCtl::setProgressEnabled(bool show)
{
    snv->barWaitProgress->setVisible(show);
}

void CSnWaitCtl::setProgressValue(int val)
{
    snv->barWaitProgress->setValue(val);
}
