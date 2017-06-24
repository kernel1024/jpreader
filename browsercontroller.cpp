#include "browsercontroller.h"
#include "globalcontrol.h"
#include "snviewer.h"

CBrowserController::CBrowserController(QObject *parent) : QObject(parent)
{

}

void CBrowserController::openUrl(const QString &url)
{
    QUrl u = QUrl::fromUserInput(url);
    if (!u.isValid()) return;

    new CSnippetViewer(gSet->activeWindow,u);
    gSet->activeWindow->showNormal();
    gSet->activeWindow->raise();
    gSet->activeWindow->activateWindow();
}

void CBrowserController::openDefaultSearch(const QString &text)
{
    QUrl url = gSet->createSearchUrl(text);
    new CSnippetViewer(gSet->activeWindow,url);
    gSet->activeWindow->showNormal();
    gSet->activeWindow->raise();
    gSet->activeWindow->activateWindow();
}
