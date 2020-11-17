#ifndef SNCTXHANDLER_H
#define SNCTXHANDLER_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QWebEngineContextMenuData>
#include <QMenu>
#include <QScopedPointer>
#include "browser.h"
#include "utils/specwidgets.h"

class CBrowserTab;

class CBrowserCtxHandler : public QObject
{
    Q_OBJECT
private:
    CBrowserTab *snv;
    QTimer m_menuActive;
public:
    explicit CBrowserCtxHandler(CBrowserTab *parent);
    void reconfigureDefaultActions();
    bool isMenuActive();
public Q_SLOTS:
    void translateFragment();
    void saveToFile();
    void bookmarkPage();
    void showInEditor();
    void showSource();
    void exportCookies();
    void runJavaScript();
    void contextMenu(const QPoint &pos, const QWebEngineContextMenuData &data);
Q_SIGNALS:
    void hideTooltips();
};

#endif // SNCTXHANDLER_H
