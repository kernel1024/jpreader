#ifndef SNCTXHANDLER_H
#define SNCTXHANDLER_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QWebEngineContextMenuData>
#include "snviewer.h"
#include "specwidgets.h"

class CSnippetViewer;

class CSnCtxHandler : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
public:
    QTimer *menuActive;
    CSnCtxHandler(CSnippetViewer *parent);
    void contextMenu(const QPoint &pos, const QWebEngineContextMenuData &data);
    void reconfigureDefaultActions();
public slots:
    void translateFragment();
    void saveToFile();
    void bookmarkPage();
    void showInEditor();
    void showSource();
signals:
    void startTranslation(bool deleteAfter);
    void hideTooltips();
};

#endif // SNCTXHANDLER_H
