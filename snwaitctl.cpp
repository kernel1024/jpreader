#include "snwaitctl.h"

CSnWaitCtl::CSnWaitCtl(CSnippetViewer *parent) :
    QObject(parent)
{
    snv = parent;
    setText(QString());
    setLanguage(QString());
    snv->lblWaitLanguage->hide();
    setProgressValue(0);
    setProgressEnabled(true);
}

void CSnWaitCtl::setText(const QString& aMsg)
{
    snv->lblWaitMessage->setText(aMsg);
}

void CSnWaitCtl::setLanguage(const QString &aLang)
{
    snv->lblWaitLanguage->setText(aLang);
    if (aLang.isEmpty()) {
        if (snv->lblWaitLanguage->isVisible())
            snv->lblWaitLanguage->hide();
    } else {
        QFont font = snv->lblWaitMessage->font();
        font.setPointSize(font.pointSize()-2);
        snv->lblWaitLanguage->setFont(font);
        if (!snv->lblWaitLanguage->isVisible())
            snv->lblWaitLanguage->show();
    }
}

void CSnWaitCtl::setProgressEnabled(bool show)
{
    snv->barWaitProgress->setVisible(show);
}

void CSnWaitCtl::setProgressValue(int val)
{
    snv->barWaitProgress->setValue(val);
}
