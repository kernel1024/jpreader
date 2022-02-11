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

class CBrowserTab;

class CBrowserNet : public QObject
{
    Q_OBJECT
private:
    CBrowserTab *snv;
    QUrl m_loadedUrl;
    QTimer m_finishedTimer;

public:
    explicit CBrowserNet(CBrowserTab * parent);
    bool isValidLoadedUrl(const QUrl& url);
    bool isValidLoadedUrl();
    QUrl getLoadedUrl() const { return m_loadedUrl; }

Q_SIGNALS:
    void startPdfConversion(const QString& filename);

public Q_SLOTS:
    void load(const QUrl & url, bool autoTranslate, bool alternateAutoTranslate);
    void load(const QString & html, const QUrl& baseUrl,
              bool autoTranslate, bool alternateAutoTranslate,
              bool onceTranslated);
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
    void pdfConverted(const QString& html);
    void pdfError(const QString& message);

};

#endif // SNNET_H
