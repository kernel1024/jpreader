#ifndef SNCTXHANDLER_H
#define SNCTXHANDLER_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QWebEngineContextMenuData>
#include <QMenu>
#include <QScopedPointer>
#include "snviewer.h"
#include "specwidgets.h"

class CSnippetViewer;

class CSnCtxHandler : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
    QTimer m_menuActive;
public:
    explicit CSnCtxHandler(CSnippetViewer *parent);
    void reconfigureDefaultActions();
    bool isMenuActive();
public Q_SLOTS:
    void translateFragment();
    void extractHTMLFragment();
    void saveToFile();
    void bookmarkPage();
    void showInEditor();
    void showSource();
    void runJavaScript();
    void contextMenu(const QPoint &pos, const QWebEngineContextMenuData &data);
Q_SIGNALS:
    void hideTooltips();
};

#endif // SNCTXHANDLER_H
