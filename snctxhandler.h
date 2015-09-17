#ifndef SNCTXHANDLER_H
#define SNCTXHANDLER_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include "snviewer.h"

class CSnippetViewer;

class CSnCtxHandler : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
public:
    QTimer menuActive;
    CSnCtxHandler(CSnippetViewer *parent);
public slots:
    void contextMenu(const QPoint &pos);
    void translateFragment();
    void saveToFile();
    void bookmarkPage();
    void loadKwrite();
signals:
    void startTranslation();
    void hideTooltips();
};

#endif // SNCTXHANDLER_H
