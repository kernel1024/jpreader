#ifndef SNCTXHANDLER_H
#define SNCTXHANDLER_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QMenu>
#include <QScopedPointer>
#include "browser.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
#include <QWebEngineContextMenuData>
#else
#include <QWebEngineContextMenuRequest>
#endif

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
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    void contextMenu(const QPoint &pos, const QWebEngineContextMenuData &data);
#else
    void contextMenu(const QPoint &pos, const QWebEngineContextMenuRequest *data);
#endif
Q_SIGNALS:
    void hideTooltips();
};

#endif // SNCTXHANDLER_H
