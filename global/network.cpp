#include <algorithm>
#include <QMessageBox>

#include "network.h"
#include "control.h"
#include "control_p.h"
#include "browserfuncs.h"
#include "translator/translatorcache.h"
#include "utils/genericfuncs.h"

CGlobalNetwork::CGlobalNetwork(QObject *parent)
    : QObject(parent)
{
}

void CGlobalNetwork::cookieAdded(const QNetworkCookie &cookie)
{
    if (gSet->d_func()->auxNetManager->cookieJar())
        gSet->d_func()->auxNetManager->cookieJar()->insertCookie(cookie);
}

void CGlobalNetwork::cookieRemoved(const QNetworkCookie &cookie)
{
    if (gSet->d_func()->auxNetManager->cookieJar())
        gSet->d_func()->auxNetManager->cookieJar()->deleteCookie(cookie);
}

bool CGlobalNetwork::sslCertErrors(const QSslCertificate &cert, const QStringList &errors,
                                   const CIntList &errCodes)
{
    if (gSet->d_func()->sslCertErrorInteractive) return false;

    QMessageBox mbox;
    mbox.setWindowTitle(QGuiApplication::applicationDisplayName());
    mbox.setText(tr("SSL error(s) occured while connecting to service.\n"
                    "Add this certificate to trusted list?"));
    QString imsg;
    for(int i=0;i<errors.count();i++)
        imsg+=QSL("%1. %2\n").arg(i+1).arg(errors.at(i));
    mbox.setInformativeText(imsg);

    mbox.setDetailedText(cert.toText());

    mbox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    mbox.setDefaultButton(QMessageBox::No);

    gSet->d_func()->sslCertErrorInteractive = true;
    auto res = mbox.exec();
    if (res == QMessageBox::Yes) {
        for (const int errCode : qAsConst(errCodes)) {
            if (!gSet->m_settings->sslTrustedInvalidCerts[cert].contains(errCode))
                gSet->m_settings->sslTrustedInvalidCerts[cert].append(errCode);
        }
    }
    gSet->d_func()->sslCertErrorInteractive = false;
    return (res == QMessageBox::Yes);
}

void CGlobalNetwork::addFavicon(const QString &key, const QIcon &icon)
{
    gSet->d_func()->favicons[key] = icon;
}

QList<QSslError> CGlobalNetwork::ignoredSslErrorsList() const
{
    QList<QSslError> expectedErrors;
    for (auto it = gSet->m_settings->sslTrustedInvalidCerts.constBegin(),
         end = gSet->m_settings->sslTrustedInvalidCerts.constEnd(); it != end; ++it) {
        for (auto iit = it.value().constBegin(), iend = it.value().constEnd(); iit != iend; ++iit) {
            expectedErrors << QSslError(static_cast<QSslError::SslError>(*iit),it.key());
        }
    }

    return expectedErrors;
}

bool CGlobalNetwork::isHostInDomainsList(const QUrl &url, const QStringList &domains) const
{
    const QString hostname = url.host();
    return std::any_of(domains.constBegin(),domains.constEnd(),[hostname](const QString& domain){
        return hostname.endsWith(domain);
    });
}

QNetworkReply *CGlobalNetwork::auxNetworkAccessManagerHead(const QNetworkRequest &request, bool bypassHttp2Suppression) const
{
    QNetworkRequest req = request;
    if (!gSet->m_settings->userAgent.isEmpty())
        req.setRawHeader("User-Agent", gSet->m_settings->userAgent.toLatin1());
    if (!bypassHttp2Suppression && isHostInDomainsList(request.url(),CStructures::incompatibleHttp2Urls()))
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    QNetworkReply *res = gSet->d_func()->auxNetManager->head(req);
    res->ignoreSslErrors(ignoredSslErrorsList());
    return res;
}

QNetworkReply *CGlobalNetwork::auxNetworkAccessManagerGet(const QNetworkRequest &request, bool bypassHttp2Suppression) const
{
    QNetworkRequest req = request;
    if (!gSet->m_settings->userAgent.isEmpty())
        req.setRawHeader("User-Agent", gSet->m_settings->userAgent.toLatin1());
    if (!bypassHttp2Suppression && isHostInDomainsList(request.url(),CStructures::incompatibleHttp2Urls()))
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    QNetworkReply *res = gSet->d_func()->auxNetManager->get(req);
    res->ignoreSslErrors(ignoredSslErrorsList());
    return res;
}

QNetworkReply *CGlobalNetwork::auxNetworkAccessManagerPost(const QNetworkRequest &request, const QByteArray &data,
                                                           bool bypassHttp2Suppression) const
{
    QNetworkRequest req = request;
    if (!gSet->m_settings->userAgent.isEmpty())
        req.setRawHeader("User-Agent", gSet->m_settings->userAgent.toLatin1());
    if (!bypassHttp2Suppression && isHostInDomainsList(request.url(),CStructures::incompatibleHttp2Urls()))
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    QNetworkReply *res = gSet->d_func()->auxNetManager->post(req,data);
    res->ignoreSslErrors(ignoredSslErrorsList());
    return res;
}

void CGlobalNetwork::authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator) const
{
    QString user;
    QString pass;
    gSet->m_browser->readPassword(reply->url(),authenticator->realm(),user,pass);
    if (!user.isEmpty()) {
        authenticator->setUser(user);
        authenticator->setPassword(pass);
    } else {
        *authenticator = QAuthenticator();
    }
}

void CGlobalNetwork::proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator) const
{
    QString user;
    QString pass;
    gSet->m_browser->readPassword(proxy.hostName(),authenticator->realm(),user,pass);
    if (!user.isEmpty()) {
        authenticator->setUser(user);
        authenticator->setPassword(pass);
    } else {
        *authenticator = QAuthenticator();
    }
}

void CGlobalNetwork::auxSSLCertError(QNetworkReply *reply, const QList<QSslError> &errors)
{
    QHash<QSslCertificate,QStringList> errStrHash;
    QHash<QSslCertificate,CIntList> errIntHash;

    for (const QSslError& err : qAsConst(errors)) {
        if (gSet->settings()->sslTrustedInvalidCerts.contains(err.certificate()) &&
                gSet->settings()->sslTrustedInvalidCerts.value(err.certificate()).contains(
                    static_cast<int>(err.error()))) continue;

        qCritical() << "Aux SSL error: " << err.errorString();

        errStrHash[err.certificate()].append(err.errorString());
        errIntHash[err.certificate()].append(static_cast<int>(err.error()));
    }

    for (auto it = errStrHash.constBegin(), end = errStrHash.constEnd(); it != end; ++it) {
        if (!sslCertErrors(it.key(),it.value(),errIntHash.value(it.key())))
            return;
    }

    reply->ignoreSslErrors();
}

void CGlobalNetwork::updateProxy(bool useProxy)
{
    updateProxyWithMenuUpdate(useProxy, false);
}

void CGlobalNetwork::updateProxyWithMenuUpdate(bool useProxy, bool forceMenuUpdate)
{
    gSet->m_settings->proxyUse = useProxy;

    QNetworkProxy proxy = QNetworkProxy();

    if (gSet->m_settings->proxyUse && !gSet->m_settings->proxyHost.isEmpty()) {
        proxy = QNetworkProxy(gSet->m_settings->proxyType,gSet->m_settings->proxyHost,
                              gSet->m_settings->proxyPort,gSet->m_settings->proxyLogin,
                              gSet->m_settings->proxyPassword);
    }

    QNetworkProxy::setApplicationProxy(proxy);

    if (forceMenuUpdate && !gSet->m_actions.isNull())
        gSet->m_actions->actionUseProxy->setChecked(gSet->m_settings->proxyUse);
}

void CGlobalNetwork::clearCaches()
{
    gSet->d_func()->webProfile->clearHttpCache();
    gSet->d_func()->translatorCache->clearCache();
}

void CGlobalNetwork::clearTranslatorCache()
{
    gSet->d_func()->translatorCache->clearCache();
}

QUrl CGlobalNetwork::createSearchUrl(const QString& text, const QString& engine) const
{
    if (gSet->d_func()->ctxSearchEngines.isEmpty())
        return QUrl(QSL("about://blank"));

    auto it = gSet->d_func()->ctxSearchEngines.constKeyValueBegin();
    QString url = (*it).second;
    if (engine.isEmpty() && !gSet->m_settings->defaultSearchEngine.isEmpty())
        url = gSet->d_func()->ctxSearchEngines.value(gSet->m_settings->defaultSearchEngine);
    if (!engine.isEmpty() && gSet->d_func()->ctxSearchEngines.contains(engine))
        url = gSet->d_func()->ctxSearchEngines.value(engine);

    url.replace(QSL("%s"),text);
    url.replace(QSL("%ps"),QString::fromUtf8(QUrl::toPercentEncoding(text)));

    return QUrl::fromUserInput(url);
}

bool CGlobalNetwork::exportCookies(const QString &filename, const QUrl &baseUrl, const QList<int>& cookieIndexes)
{
    if (filename.isEmpty() ||
            (baseUrl.isEmpty() && cookieIndexes.isEmpty()) ||
            (!baseUrl.isEmpty() && !cookieIndexes.isEmpty())) return false;

    auto *cj = qobject_cast<CNetworkCookieJar *>(gSet->d_func()->auxNetManager->cookieJar());
    if (cj==nullptr) return false;
    QList<QNetworkCookie> cookiesList;
    if (baseUrl.isEmpty()) {
        const auto all = cj->getAllCookies();
        cookiesList.reserve(cookieIndexes.count());
        for (const auto idx : cookieIndexes)
            cookiesList.append(all.at(idx));
    } else {
        cookiesList = cj->cookiesForUrl(baseUrl);
    }

    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QTextStream fs(&f);

    for (const auto &cookie : qAsConst(cookiesList)) {
        fs << cookie.domain()
           << u'\t'
           << CGenericFuncs::bool2str2(cookie.domain().startsWith(u'.'))
           << u'\t'
           << cookie.path()
           << u'\t'
           << CGenericFuncs::bool2str2(cookie.isSecure())
           << u'\t'
           << cookie.expirationDate().toSecsSinceEpoch()
           << u'\t'
           << cookie.name()
           << u'\t'
           << cookie.value()
           << Qt::endl;
    }
    fs.flush();
    f.close();

    return true;
}

void CGlobalNetwork::addTranslatorStatistics(CStructures::TranslationEngine engine, int textLength)
{
    if (textLength < 0) {
        Q_EMIT translationStatisticsChanged();
    } else {
        gSet->m_settings->translatorStatistics[engine][QDate::currentDate()] += static_cast<quint64>(textLength);
    }
}

QString CGlobalNetwork::getLanguageName(const QString& bcp47Name) const
{
    return gSet->d_func()->langNamesList.value(bcp47Name);
}

QStringList CGlobalNetwork::getLanguageCodes() const
{
    return gSet->d_func()->langSortedBCP47List.values();
}

QStringList CGlobalNetwork::getSearchEngines() const
{
    return gSet->d_func()->ctxSearchEngines.keys();
}

void CGlobalNetwork::setTranslationEngine(CStructures::TranslationEngine engine)
{
    gSet->m_settings->setTranslationEngine(engine);
}

void CGlobalNetwork::addPixivCommonCover(const QString &url, const QString &data)
{
    QMutexLocker locker(&(gSet->d_func()->pixivCommonCoversMutex));
    gSet->d_func()->pixivCommonCovers.insert(url,data);
}

QString CGlobalNetwork::getPixivCommonCover(const QString &url) const
{
    QMutexLocker locker(&(gSet->d_func()->pixivCommonCoversMutex));
    return gSet->d_func()->pixivCommonCovers.value(url);
}
