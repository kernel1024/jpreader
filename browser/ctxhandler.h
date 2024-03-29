#ifndef SNCTXHANDLER_H
#define SNCTXHANDLER_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QMenu>
#include <QScopedPointer>
#include "browser.h"

#include <QWebEngineContextMenuRequest>

class CBrowserTab;

class CBrowserCtxHandler : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CBrowserCtxHandler)
private:
    CBrowserTab *snv;
    QTimer m_menuActive;
public:
    explicit CBrowserCtxHandler(CBrowserTab *parent);
    ~CBrowserCtxHandler() override = default;
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
    void contextMenu(const QPoint &pos, const QWebEngineContextMenuRequest *data);
Q_SIGNALS:
    void hideTooltips();
};

#endif // SNCTXHANDLER_H
