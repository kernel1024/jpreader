#ifndef SNWAITCTL_H
#define SNWAITCTL_H

#include <QObject>
#include <QString>
#include "snviewer.h"

class CSnWaitCtl : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer* snv;
public:
    explicit CSnWaitCtl(CSnippetViewer *parent);
    void setText(const QString &aMsg);
    void setLanguage(const QString &aLang);
public Q_SLOTS:
    void setProgressEnabled(bool show);
    void setProgressValue(int val);

};

#endif // SNWAITCTL_H
