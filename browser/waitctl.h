#ifndef SNWAITCTL_H
#define SNWAITCTL_H

#include <QObject>
#include <QString>
#include "browser.h"

class CBrowserWaitCtl : public QObject
{
    Q_OBJECT
private:
    CBrowserTab* snv;
public:
    explicit CBrowserWaitCtl(CBrowserTab *parent);
    void setText(const QString &aMsg);
    void setLanguage(const QString &aLang);
public Q_SLOTS:
    void setProgressEnabled(bool show);
    void setProgressValue(int val);

};

#endif // SNWAITCTL_H
