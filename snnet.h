#ifndef SNNET_H
#define SNNET_H

#include <QObject>
#include <QUrl>
#include <QWebFrame>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMutex>
#include <QList>
#include "snviewer.h"

class CSnNet : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
    void showErrorMsg(QNetworkReply* reply);
    QList<QUrl> mlsBaseUrls;
    QMutex mtxBaseUrls;
public:
    CSnNet(CSnippetViewer * parent);
    QUrl fixUrl(QUrl aUrl);
    bool isUrlNowProcessing(const QUrl &url);
    void addUrlToProcessing(const QUrl &url);
    void removeUrlFromProcessing(const QUrl &url);
public slots:
    void loadProcessed(const QUrl & url, QWebFrame* frame = NULL,
                       QNetworkRequest::CacheLoadControl ca = QNetworkRequest::PreferNetwork);
    void netDlProgress(qint64 bytesReceived, qint64 bytesTotal);
    void netHtmlLoaded();
    void reloadMedia(bool fromNew = false);
    void loadStarted();
    void loadFinished(bool);
    void loadError(QNetworkReply::NetworkError code);
    void netStop();
signals:
    void closeAllSockets();

};

#endif // SNNET_H
