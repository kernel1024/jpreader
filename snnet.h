#ifndef SNNET_H
#define SNNET_H

#include <QObject>
#include <QUrl>
#include "snviewer.h"

class CSnNet : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
public:
    QUrl loadedUrl;
    CSnNet(CSnippetViewer * parent);
public slots:
    void load(const QUrl & url);
    void load(const QString & html, const QUrl& loadedUrl = QUrl());
    void authenticationRequired(const QUrl& requestUrl, QAuthenticator* authenticator);
    void proxyAuthenticationRequired(const QUrl & requestUrl, QAuthenticator * authenticator,
                                     const QString & proxyHost);
    void loadStarted();
    void loadFinished(bool);
    void userNavigationRequest(const QUrl& url, const int type, const bool isMainFrame);
    void iconUrlChanged(const QUrl &url);
    void urlIconFinished();

};

#endif // SNNET_H
