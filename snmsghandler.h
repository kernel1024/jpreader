#ifndef SNMSGHANDLER_H
#define SNMSGHANDLER_H

#include <QObject>
#include <QUrl>
#include <QWebFrame>
#include <QString>
#include "snviewer.h"

class CSnippetViewer;

class CSnMsgHandler : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
public:
    CSnMsgHandler(CSnippetViewer * parent);
public slots:
    void loadHomeUri();
    void navBack();
    void navForward();
    void linkClicked(QWebFrame *frame, const QUrl &url, const QWebPage::NavigationType &clickType);
    void linkHovered(const QString &link, const QString &, const QString &);
    void searchFwd();
    void searchBack();
    void setZoom(QString z);
    void urlEdited(const QString &url);
    void navByClick();
};

#endif // SNMSGHANDLER_H
