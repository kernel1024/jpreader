#include "browsercontroller.h"
#include "globalcontrol.h"
#include "snviewer.h"

CBrowserController::CBrowserController(QObject *parent) : QObject(parent)
{

}

void CBrowserController::openUrl(const QString &url)
{
    CMainWindow* w = gSet->activeWindow();
    if (w==nullptr) return;

    QUrl u = QUrl::fromUserInput(url);
    if (!u.isValid()) return;

    new CSnippetViewer(w,u);
    w->showNormal();
    w->raise();
    w->activateWindow();
}

void CBrowserController::openDefaultSearch(const QString &text)
{
    CMainWindow* w = gSet->activeWindow();
    if (w==nullptr) return;

    QUrl url = gSet->createSearchUrl(text);
    new CSnippetViewer(w,url);
    w->showNormal();
    w->raise();
    w->activateWindow();
}
