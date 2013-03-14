#ifndef SNNET_H
#define SNNET_H

#include <QObject>
#include <QUrl>
#include <QWebFrame>
#include <QNetworkRequest>
#include "snviewer.h"

class CSnNet : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
public:
    CSnNet(CSnippetViewer * parent);
    QUrl fixUrl(QUrl aUrl);
public slots:
    void loadProcessed(const QUrl & url, QWebFrame* frame = NULL,
                       QNetworkRequest::CacheLoadControl ca = QNetworkRequest::PreferNetwork);
    void netDlProgress(qint64 bytesReceived, qint64 bytesTotal);
    void netHtmlLoaded();
    void reloadMedia(bool fromNew = false);
    void loadStarted();
    void loadFinished(bool);
    void netStop();
signals:
    void closeAllSockets();

};

#endif // SNNET_H
