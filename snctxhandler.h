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
    QTimer *menuActive;
public:
    explicit CSnCtxHandler(CSnippetViewer *parent);
    void contextMenu(QPoint pos, const QWebEngineContextMenuData &data);
    void reconfigureDefaultActions();
    bool isMenuTimerActive();
public Q_SLOTS:
    void translateFragment();
    void saveToFile();
    void bookmarkPage();
    void showInEditor();
    void showSource();
    void runJavaScript();
Q_SIGNALS:
    void startTranslation(bool deleteAfter);
    void hideTooltips();
};

#endif // SNCTXHANDLER_H
