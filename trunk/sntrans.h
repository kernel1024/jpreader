#ifndef SNTRANS_H
#define SNTRANS_H

#include <QtCore>
#include <QtGui>
#include <QtWebKit>
#include "snviewer.h"

class CSnTrans : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
public:
    CSnTrans(CSnippetViewer * parent);
public slots:
    void translate();
    void calcFinished(bool success, QString aUrl);
    void postTranslate();
    void progressLoad(int progress);
};

#endif // SNTRANS_H
