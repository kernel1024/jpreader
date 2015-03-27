#include <QMessageBox>
#include "snmsghandler.h"

CSnMsgHandler::CSnMsgHandler(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
}

void CSnMsgHandler::searchFwd()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;
    snv->txtBrowser->findText(snv->searchEdit->currentText(), 0);
}

void CSnMsgHandler::searchBack()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;
    snv->txtBrowser->findText(snv->searchEdit->currentText(), QWebPage::FindBackward);
}

void CSnMsgHandler::setZoom(QString z)
{
    if (snv->txtBrowser) {
        bool okconv;
        int i = z.remove(QRegExp("[^0-9]")).toInt(&okconv);
        if (okconv)
            snv->txtBrowser->setZoomFactor(((double)i)/100);
    }
}

void CSnMsgHandler::urlEdited(const QString &)
{
}

void CSnMsgHandler::navByClick()
{
    if (snv->urlEdit->currentText().isEmpty()) return;
    snv->navByUrl(snv->urlEdit->currentText());
}

void CSnMsgHandler::navBack()
{
    if (snv->backHistory.count()==0) return;
    snv->forwardStack.prepend(snv->backHistory.takeLast()); // move current url;
    snv->netHandler->loadProcessed(snv->backHistory.takeLast(),NULL,QNetworkRequest::PreferCache);
}

void CSnMsgHandler::navForward()
{
    if (snv->forwardStack.count()==0) return;
    snv->netHandler->loadProcessed(snv->forwardStack.takeFirst(),NULL,QNetworkRequest::PreferCache);
}

void CSnMsgHandler::loadHomeUri()
{
    if (!snv->firstUri.isEmpty())
        snv->netHandler->loadProcessed(snv->firstUri);
    else
        QMessageBox::warning(snv,tr("JPReader"),tr("Unable to return to dynamically generated first page."));
}

void CSnMsgHandler::linkClicked(QWebFrame * frame, const QUrl &url, const QWebPage::NavigationType &clickType)
{
    QUrl u = url;
    if (u.isRelative() && frame!=NULL) {
        QUrl uu = frame->requestedUrl();
        if (uu.isEmpty() || uu.isRelative())
            uu = snv->Uri;
        u = uu.resolved(u);
    }
    u = snv->netHandler->fixUrl(u);
    if (u.isEmpty()) {
        QMessageBox::warning(snv,tr("JPReader"),tr("Url is invalid"));
        return;
    }
    if (gSet->forceAllLinksInNewTab() && clickType==QWebPage::NavigationTypeLinkClicked)
        new CSnippetViewer(snv->parentWnd, u, QStringList(), false);
    else {
        if (clickType==QWebPage::NavigationTypeLinkClicked)
            snv->forwardStack.clear();
        snv->netHandler->loadProcessed(u,frame);
    }
}

void CSnMsgHandler::linkHovered(const QString &link, const QString &, const QString &)
{
    snv->statusBarMsg(link);
}