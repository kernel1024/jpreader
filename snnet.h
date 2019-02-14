#ifndef SNNET_H
#define SNNET_H

#include <QObject>
#include <QUrl>
#include <QPixmap>
#include <QWebEngineScript>
#include <QNetworkReply>

class CSnippetViewer;

class CSnNet : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
public:
    QUrl loadedUrl;
    CSnNet(CSnippetViewer * parent);
    void multiImgDownload(const QStringList& urls, const QUrl &referer);
    bool isValidLoadedUrl(const QUrl& url);
    bool isValidLoadedUrl();
    bool loadWithTempFile(const QString & html, bool createNewTab);
signals:
    void startPdfConversion(const QString& filename);
public slots:
    void load(const QUrl & url);
    void load(const QString & html, const QUrl& loadedUrl = QUrl());
    void authenticationRequired(const QUrl& requestUrl, QAuthenticator* authenticator);
    void proxyAuthenticationRequired(const QUrl & requestUrl, QAuthenticator * authenticator,
                                     const QString & proxyHost);
    void loadStarted();
    void loadFinished(bool ok);
    void userNavigationRequest(const QUrl& url, const int type, const bool isMainFrame);
    void processPixivNovel(const QUrl& url, const QString &title, bool translate, bool focus);
    void pixivNovelReady(const QString& html, bool focus, bool translate);
    void pdfConverted(const QString& html);
    void pdfError(const QString& message);

};

#endif // SNNET_H
