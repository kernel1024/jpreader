#include <QTextCodec>
#include <QPointer>
#include "snnet.h"
#include "genericfuncs.h"

Q_DECLARE_METATYPE(QPointer<QWebFrame>)

#define QV_FramePtr QVariant::UserType+8

CSnNet::CSnNet(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    mlsBaseUrls.clear();
}

QUrl CSnNet::fixUrl(QUrl aUrl)
{
    QByteArray u = aUrl.toEncoded();
    if (u.isEmpty())
        return QUrl("");
    // some common URL bug fixes
    u = u.replace("%E2%80%BE","~");  // ~
    u = u.replace("%C2%A5","/"); // backslash to slash
    return QUrl::fromUserInput(QString::fromUtf8(u));
}

bool CSnNet::isUrlNowProcessing(const QUrl &url)
{
    mtxBaseUrls.lock();
    bool a = mlsBaseUrls.contains(url);
    mtxBaseUrls.unlock();
    return a;
}

void CSnNet::addUrlToProcessing(const QUrl &url)
{
    mtxBaseUrls.lock();
    mlsBaseUrls << url;
    mtxBaseUrls.unlock();
}

void CSnNet::removeUrlFromProcessing(const QUrl &url)
{
    mtxBaseUrls.lock();
    mlsBaseUrls.removeAll(url);
    mtxBaseUrls.unlock();
}

void CSnNet::loadStarted()
{
    snv->barLoading->setValue(0);
    snv->barLoading->show();
    snv->barPlaceholder->hide();
    snv->loadingByWebKit = true;
    snv->updateButtonsState();
}

void CSnNet::loadFinished(bool)
{
    QUrl base = snv->txtBrowser->url();
    if (!base.isEmpty() && base.isLocalFile() && base!=snv->Uri && !snv->Uri.isEmpty() && !snv->waitPanel->isVisible()) {
        // Spontaneous loading from WebView for local files
        snv->onceTranslated = false;
        snv->fileChanged = false;
        snv->backHistory.append(base);
        snv->Uri = base;
        snv->urlEdit->setToolTip(tr("Displayed URL"));
        snv->urlEdit->setPalette(QApplication::palette(snv->urlEdit));
    }

    snv->msgHandler->hideBarLoading();
    snv->loadingByWebKit = false;
    snv->updateButtonsState();

    if (snv->parentWnd->tabMain->currentIndex()!=snv->parentWnd->tabMain->indexOf(snv) &&
            !snv->translationBkgdFinished) { // tab is inactive
        snv->loadingBkgdFinished=true;
        snv->parentWnd->tabMain->tabBar()->setTabTextColor(snv->parentWnd->tabMain->indexOf(snv),QColor("blue"));
        snv->parentWnd->updateTabs();
    }

    if ((gSet->autoTranslate() || snv->requestAutotranslate) && !snv->onceTranslated) {
        snv->requestAutotranslate = false;
        snv->transButton->click();
    }
}

void CSnNet::showErrorMsg(QNetworkReply *reply)
{
    if (reply==NULL) return;
    if (reply->error()==QNetworkReply::NoError) return;
    QString errorString;
    if (reply!=NULL) {
        errorString = QString("<b>%1</b><br/>").arg(reply->errorString());
        errorString += QString("Network error #%1.<br/>").arg(reply->error());
        errorString += QString("URL: %1").arg(reply->request().url().toString());
    } else
        errorString = QString("<b>Network Error</b>");
    snv->errorLabel->setText(errorString);
    snv->errorPanel->show();
    if (snv->tabTitle.isEmpty())
        snv->titleChanged(tr("Network error"));
}

void CSnNet::loadError(QNetworkReply::NetworkError)
{
    QNetworkReply *rpl = qobject_cast<QNetworkReply *>(sender());
    showErrorMsg(rpl);
    netStop();
}

void CSnNet::netStop()
{
    emit closeAllSockets();
    snv->loadingByWebKit = false;
    snv->updateButtonsState();
}

void CSnNet::loadProcessed(const QUrl &url, QWebFrame *frame, QNetworkRequest::CacheLoadControl ca)
{
    if (gSet->isUrlBlocked(url)) {
        qDebug() << "adblock - skipping" << url;
        return;
    }

    snv->updateWebViewAttributes();

    QNetworkRequest rq(url);
    if (frame!=NULL) {
        QPointer<QWebFrame> frameptr(frame);
        rq.setAttribute(QNetworkRequest::User,QVariant::fromValue(frameptr));
    }
    rq.setAttribute(QNetworkRequest::CacheLoadControlAttribute,ca);

    if (gSet->overrideUserAgent)
        rq.setRawHeader("User-Agent",gSet->userAgent.toLatin1());

    QNetworkReply* rpl = snv->txtBrowser->page()->networkAccessManager()->get(rq);
    connect(rpl,SIGNAL(finished()),this,SLOT(netHtmlLoaded()));
    connect(rpl,SIGNAL(downloadProgress(qint64,qint64)),this,SLOT(netDlProgress(qint64,qint64)));
    connect(rpl,SIGNAL(error(QNetworkReply::NetworkError)),
            this,SLOT(loadError(QNetworkReply::NetworkError)));
    connect(this,SIGNAL(closeAllSockets()),rpl,SLOT(deleteLater()));

    snv->updateButtonsState();

    if (!snv->barLoading->isVisible()) loadStarted();
    snv->onceTranslated=false;
    snv->translationBkgdFinished=false;
    snv->loadingBkgdFinished=false;
    snv->isStartPage=false;

    UrlHolder uh(snv->getDocTitle(),url);
    gSet->appendMainHistory(uh);
}

void CSnNet::netDlProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal!=0)
        snv->barLoading->setValue(100*bytesReceived/bytesTotal);
    else
        snv->barLoading->setValue(0);
    return;
}

void CSnNet::netHtmlLoaded()
{
    QNetworkReply* rpl = qobject_cast<QNetworkReply*>(sender());
    if (rpl==NULL) return;
    if (rpl->error()!=QNetworkReply::NoError) {
        showErrorMsg(rpl);
        snv->updateButtonsState();
        if (!snv->loadingByWebKit) loadFinished(true);
        return;
    }
    if (!rpl->isOpen()) return;
    QUrl base = rpl->url();

    QPointer<QWebFrame> frm;
    QVariant vf = rpl->request().attribute(QNetworkRequest::User,QVariant());
    if (vf.canConvert<QPointer<QWebFrame> >())
        frm = vf.value<QPointer<QWebFrame> >();

    QByteArray ba = rpl->readAll();
    QString mime;
    QVariant redirect;
    if (base.toLocalFile().isEmpty()) {
        mime = rpl->header(QNetworkRequest::ContentTypeHeader).toString();
        redirect = rpl->header(QNetworkRequest::LocationHeader);
    } else {
        mime = detectMIME(ba);
    }

    if (redirect.canConvert<QUrl>()) {
        loadProcessed(redirect.toUrl(),frm);
    } else {
        if (frm==NULL || frm==snv->txtBrowser->page()->mainFrame()) {
            snv->backHistory.append(base);
            snv->Uri = base;
            snv->fileChanged = false;
        }

        if (mime.contains("image",Qt::CaseInsensitive)) {
            QString cn="<img src=\""+rpl->url().toString()+"\" />";
            QFileInfo fi(rpl->url().path());
            QString ft = fi.baseName();
            if (!fi.completeSuffix().isEmpty()) ft+="."+fi.completeSuffix();
            snv->txtBrowser->settings()->setAttribute(QWebSettings::AutoLoadImages,true);
            snv->txtBrowser->setHtml(makeSimpleHtml(ft,cn));
        } else {
            QTextCodec* enc = detectEncoding(ba);

            addUrlToProcessing(base);

            if (frm!=NULL)
                frm->setHtml(fixMetaEncoding(enc->toUnicode(ba)),base);
            else
                snv->txtBrowser->setHtml(fixMetaEncoding(enc->toUnicode(ba)),base);

            removeUrlFromProcessing(base);
        }
    }
    snv->updateButtonsState();
    if (!snv->loadingByWebKit) loadFinished(true);
}

void CSnNet::reloadMedia(bool fromNew)
{
    bool forceReload = false;
    if (sender()!=NULL) {
        if (sender()->objectName().contains("reload",Qt::CaseInsensitive))
            forceReload = true;
    }
    QStringList supportedImgs, supportedTxt;
    supportedImgs << "image/x-ms-bmp" << "image/gif" << "image/jpeg" << "image/png" <<
                     "image/x-portable-bitmap" << "image/x-portable-graymap" <<
                     "image/x-portable-pixmap" << "image/x-xbitmap" << "image/x-xpixmap";
    supportedTxt << "text/comma-separated-values" << "text/css" << "text/plain" << "text/x-sh" <<
                    "text/x-python" << "text/x-perl" << "text/x-pascal" << "text/x-c++hdr" <<
                    "text/x-c++src" << "text/x-chdr" << "text/x-haskell" << "text/x-java" <<
                    "text/x-makefile";

    QString fname = snv->Uri.toLocalFile();
    snv->updateWebViewAttributes();
    if (!fromNew && snv->isStartPage)
        snv->isStartPage = false;

    if (!fname.isEmpty()) {
        QString MIME = detectMIME(fname);
        if (supportedImgs.contains(MIME, Qt::CaseInsensitive)) { // for local image files
            QFileInfo fi(fname);
            QString cn="<img src=\""+QString::fromUtf8(snv->Uri.toEncoded())+"\" />";
            snv->fileChanged = false;
            snv->translationBkgdFinished=false;
            snv->loadingBkgdFinished=false;
            snv->txtBrowser->settings()->setAttribute(QWebSettings::AutoLoadImages,true);
            snv->txtBrowser->setHtml(makeSimpleHtml(fi.fileName(),cn));
        } else if (supportedTxt.contains(MIME, Qt::CaseInsensitive)) { // for local txt files
            QFile data(fname);
            QFileInfo fi(fname);
            if (data.open(QFile::ReadOnly)) {
                QByteArray ba = data.readAll();
                QTextCodec* cd = detectEncoding(ba);
                QString cn=makeSimpleHtml(fi.fileName(),cd->toUnicode(ba));
                snv->fileChanged = false;
                snv->translationBkgdFinished=false;
                snv->loadingBkgdFinished=false;
                snv->txtBrowser->setHtml(cn);
                data.close();
            }
        } else { // for local html files
            snv->fileChanged = false;
            loadProcessed(snv->Uri);
        }
    } else if (!snv->auxContent.isEmpty()) {
        snv->fileChanged = false;
        snv->Uri.clear();
        snv->txtBrowser->setHtml(snv->auxContent);
    } else if (!snv->Uri.isEmpty()){
        snv->fileChanged = false;
        if (forceReload)
            loadProcessed(snv->Uri,NULL,QNetworkRequest::AlwaysNetwork);
        else
            loadProcessed(snv->Uri);
    } else
        snv->fileChanged = false;
    snv->txtBrowser->setFocus();
    if (snv->Uri.isValid())
        snv->urlEdit->setEditText(snv->Uri.toString());
}
