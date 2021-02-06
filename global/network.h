#ifndef GLOBALNETWORK_H
#define GLOBALNETWORK_H

#include <QObject>
#include <QSslCertificate>
#include <QNetworkReply>
#include <QAuthenticator>

#include "structures.h"

class CGlobalNetwork : public QObject
{
    Q_OBJECT
public:
    explicit CGlobalNetwork(QObject *parent = nullptr);

    QUrl createSearchUrl(const QString& text, const QString& engine = QString()) const;
    void addFavicon(const QString& key, const QIcon &icon);

    // Network
    QNetworkReply *auxNetworkAccessManagerHead(const QNetworkRequest &request) const;
    QNetworkReply *auxNetworkAccessManagerGet(const QNetworkRequest &request) const;
    QNetworkReply *auxNetworkAccessManagerPost(const QNetworkRequest &request, const QByteArray &data) const;
    QList<QSslError> ignoredSslErrorsList() const;

    bool exportCookies(const QString& filename = QString(), const QUrl& baseUrl = QUrl(), const QList<int> &cookieIndexes = {});
    void addTranslatorStatistics(CStructures::TranslationEngine engine, int textLength);

    QStringList getLanguageCodes() const;
    QString getLanguageName(const QString &bcp47Name) const;
    QStringList getSearchEngines() const;
    void setTranslationEngine(CStructures::TranslationEngine engine);

    void addPixivCommonCover(const QString& url, const QString& data);
    QString getPixivCommonCover(const QString& url) const;

Q_SIGNALS:
    void translationStatisticsChanged();

public Q_SLOTS:
    void updateProxy(bool useProxy);
    void updateProxyWithMenuUpdate(bool useProxy, bool forceMenuUpdate);
    void clearCaches();
    void clearTranslatorCache();

    void cookieAdded(const QNetworkCookie &cookie);
    void cookieRemoved(const QNetworkCookie &cookie);

    // ATLAS SSL
    bool sslCertErrors(const QSslCertificate& cert, const QStringList& errors,
                       const CIntList& errCodes);

    void authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator) const;
    void proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator) const;
    void auxSSLCertError(QNetworkReply *reply, const QList<QSslError> &errors);

};

#endif // GLOBALNETWORK_H
