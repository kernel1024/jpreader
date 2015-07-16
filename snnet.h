#ifndef SNNET_H
#define SNNET_H

#include <QObject>
#include "snviewer.h"

class CSnNet : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
//    void showErrorMsg(QNetworkReply* reply);
//    QList<QUrl> mlsBaseUrls;
//    QMutex mtxBaseUrls;
public:
    CSnNet(CSnippetViewer * parent);
/*    QUrl fixUrl(QUrl aUrl);
    bool isUrlNowProcessing(const QUrl &url);
    void addUrlToProcessing(const QUrl &url);
    void removeUrlFromProcessing(const QUrl &url);*/
public slots:
    void load(const QUrl & url);
    void load(const QString & html, const QUrl& baseUrl = QUrl());
    void authenticationRequired(const QUrl& requestUrl, QAuthenticator* authenticator);
    void proxyAuthenticationRequired(const QUrl & requestUrl, QAuthenticator * authenticator,
                                     const QString & proxyHost);
/*    void netDlProgress(qint64 bytesReceived, qint64 bytesTotal);
    void netHtmlLoaded();
    void reloadMedia(bool fromNew = false);
    void loadError(QNetworkReply::NetworkError code);
    void netStop();*/
    void loadStarted();
    void loadFinished(bool);
//signals:
//    void closeAllSockets();

};

#endif // SNNET_H
