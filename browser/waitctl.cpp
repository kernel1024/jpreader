#include "waitctl.h"

CBrowserWaitCtl::CBrowserWaitCtl(CBrowserTab *parent) :
    QObject(parent)
{
    snv = parent;
    setText(QString());
    setLanguage(QString());
    snv->lblWaitLanguage->hide();
    setProgressValue(0);
    setProgressEnabled(true);
}

void CBrowserWaitCtl::setText(const QString& aMsg)
{
    snv->lblWaitMessage->setText(aMsg);
}

void CBrowserWaitCtl::setLanguage(const QString &aLang)
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

void CBrowserWaitCtl::setProgressEnabled(bool show)
{
    snv->barWaitProgress->setVisible(show);
}

void CBrowserWaitCtl::setProgressValue(int val)
{
    snv->barWaitProgress->setValue(val);
}
