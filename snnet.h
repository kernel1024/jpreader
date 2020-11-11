#ifndef SNNET_H
#define SNNET_H

#include <QObject>
#include <QUrl>
#include <QPixmap>
#include <QWebEngineScript>
#include <QNetworkReply>
#include <QTimer>
#include "extractors/pixivindexextractor.h"
#include "extractors/patreonextractor.h"

class CSnippetViewer;

class CSnNet : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
    QUrl m_loadedUrl;
    QTimer m_finishedTimer;

public:
    explicit CSnNet(CSnippetViewer * parent);
    void multiFileDownload(const QVector<CUrlWithName> &urls, const QUrl &referer,
                          const QString &containerName = QString(), bool isFanbox = false,
                          bool isPatreon = false);
    bool isValidLoadedUrl(const QUrl& url);
    bool isValidLoadedUrl();
    bool loadWithTempFile(const QString & html, bool createNewTab, bool autoTranslate = false);
    QUrl getLoadedUrl() const { return m_loadedUrl; }

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
    void progressLoad(int progress);
    void userNavigationRequest(const QUrl& url, int type, bool isMainFrame);

    void extractHTMLFragment();
    void processExtractorAction();
    void processExtractorActionIndirect(const QVariantHash &params);
    void novelReady(const QString& html, bool focus, bool translate);
    void mangaReady(const QVector<CUrlWithName> &urls, const QString &containerName, const QUrl &origin);

    void pdfConverted(const QString& html);
    void pdfError(const QString& message);

};

#endif // SNNET_H
