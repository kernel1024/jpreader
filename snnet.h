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
    QUrl loadedUrl;
public:
    explicit CSnNet(CSnippetViewer * parent);
    void multiImgDownload(const QStringList& urls, const QUrl &referer);
    bool isValidLoadedUrl(const QUrl& url);
    bool isValidLoadedUrl();
    bool loadWithTempFile(const QString & html, bool createNewTab);
    QUrl getLoadedUrl() const { return loadedUrl; }

Q_SIGNALS:
    void startPdfConversion(const QString& filename);

public Q_SLOTS:
    void load(const QUrl & url);
    void load(const QString & html, const QUrl& baseUrl = QUrl());
    void authenticationRequired(const QUrl& requestUrl, QAuthenticator* authenticator);
    void proxyAuthenticationRequired(const QUrl & requestUrl, QAuthenticator * authenticator,
                                     const QString & proxyHost);
    void loadStarted();
    void loadFinished(bool ok);
    void userNavigationRequest(const QUrl& url, int type, bool isMainFrame);
    void processPixivNovel(const QUrl& url, const QString &title, bool translate, bool focus);
    void downloadPixivManga();
    void pixivNovelReady(const QString& html, bool focus, bool translate);
    void pdfConverted(const QString& html);
    void pdfError(const QString& message);

};

#endif // SNNET_H
