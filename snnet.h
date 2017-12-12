#ifndef SNNET_H
#define SNNET_H

#include <QObject>
#include <QUrl>
#include <QPixmap>
#include <QWebEngineScript>
#include "snviewer.h"

class CSnNet : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
public:
    QUrl loadedUrl;
    CSnNet(CSnippetViewer * parent);
    void multiImgDownload(const QStringList& urls, const QUrl &referer);
public slots:
    void load(const QUrl & url);
    void load(const QString & html, const QUrl& loadedUrl = QUrl());
    void authenticationRequired(const QUrl& requestUrl, QAuthenticator* authenticator);
    void proxyAuthenticationRequired(const QUrl & requestUrl, QAuthenticator * authenticator,
                                     const QString & proxyHost);
    void loadStarted();
    void loadFinished(bool);
    void userNavigationRequest(const QUrl& url, const int type, const bool isMainFrame);
    void processPixivNovel(const QUrl& url, const QString &title, bool translate);
    void pixivNovelError(QNetworkReply::NetworkError error);

};

#endif // SNNET_H
