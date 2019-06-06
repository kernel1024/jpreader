#include <QTextCodec>
#include <QPointer>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrlQuery>
#include <QWebEngineScriptCollection>
#include <QDirIterator>
#include <QRegExp>
#include <QDialog>
#include <QThread>
#include <QAuthenticator>

#include "snnet.h"
#include "snmsghandler.h"
#include "genericfuncs.h"
#include "authdlg.h"
#include "pdftotext.h"
#include "downloadmanager.h"
#include "genericfuncs.h"
#include "pixivnovelextractor.h"
#include "globalcontrol.h"
#include "ui_selectablelistdlg.h"

CSnNet::CSnNet(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
}

void CSnNet::multiImgDownload(const QStringList &urls, const QUrl& referer)
{
    static QSize multiImgDialogSize = QSize();

    QDialog *dlg = new QDialog(snv);
    Ui::SelectableListDlg ui;
    ui.setupUi(dlg);
    if (multiImgDialogSize.isValid())
        dlg->resize(multiImgDialogSize);

    connect(ui.filter,&QLineEdit::textEdited,[ui](const QString& text){
        if (text.isEmpty()) {
            for (int i=0;i<ui.list->count();i++) {
                if (ui.list->item(i)->isHidden())
                    ui.list->item(i)->setHidden(false);
            }
            return;
        }

        for (int i=0;i<ui.list->count();i++) {
            ui.list->item(i)->setHidden(true);
        }
        QList<QListWidgetItem *> filtered = ui.list->findItems(text,
                                                               ((ui.syntax->currentIndex()==0) ?
                                                                    Qt::MatchRegExp : Qt::MatchWildcard));
        for (QListWidgetItem* item : filtered) {
            item->setHidden(false);
        }
    });

    ui.label->setText(tr("Detected image URLs from page"));
    ui.syntax->setCurrentIndex(0);
    ui.list->addItems(urls);
    dlg->setWindowTitle(tr("Multiple images download"));

    if (dlg->exec()==QDialog::Accepted) {
        QString dir = CGenericFuncs::getExistingDirectoryD(snv,tr("Save images to directory"),
                                                           CGenericFuncs::getTmpDir());
        int index = 0;
        const QList<QListWidgetItem *> itml = ui.list->selectedItems();
        for (const QListWidgetItem* itm : itml){
            if (!ui.checkAddNumbers->isChecked()) {
                index = -1;
            } else {
                index++;
            }

            gSet->downloadManager()->handleAuxDownload(itm->text(),dir,referer,index,ui.list->selectedItems().count());
        }
    }
    multiImgDialogSize=dlg->size();
    dlg->setParent(nullptr);
    dlg->deleteLater();
}

bool CSnNet::isValidLoadedUrl(const QUrl& url)
{
    // loadedUrl points to non-empty page
    if (!url.isValid()) return false;
    if (!url.toLocalFile().isEmpty()) return true;
    if (url.scheme().startsWith(QStringLiteral("http"),Qt::CaseInsensitive)) return true;
    return false;
}

bool CSnNet::isValidLoadedUrl()
{
    return isValidLoadedUrl(m_loadedUrl);
}

bool CSnNet::loadWithTempFile(const QString &html, bool createNewTab)
{
    QByteArray ba = html.toUtf8();
    QString fname = gSet->makeTmpFileName(QStringLiteral("html"),true);
    QFile f(fname);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(ba);
        f.close();
        gSet->appendCreatedFiles(fname);
        if (createNewTab) {
            new CSnippetViewer(snv->parentWnd(),QUrl::fromLocalFile(fname));
        } else {
            snv->m_fileChanged = false;
            snv->txtBrowser->load(QUrl::fromLocalFile(fname));
            snv->m_auxContentLoaded=false;
        }
        return true;
    }

    QMessageBox::warning(snv,tr("JPReader"),tr("Unable to create temporary file "
                                               "for document."));
    return false;
}

void CSnNet::loadStarted()
{
    snv->barLoading->setValue(0);
    snv->barLoading->show();
    snv->barPlaceholder->hide();
    snv->m_loading = true;
    snv->updateButtonsState();
}

void CSnNet::loadFinished(bool ok)
{
    snv->msgHandler->updateZoomFactor();
    snv->msgHandler->hideBarLoading();
    snv->m_loading = false;
    snv->updateButtonsState();

    if (isValidLoadedUrl(snv->txtBrowser->url()) && ok) {
        snv->m_auxContentLoaded=false;
        m_loadedUrl = snv->txtBrowser->url();
    }

    snv->updateTabColor(true,false);

    bool xorAutotranslate = (gSet->ui()->autoTranslate() && !snv->m_requestAutotranslate) ||
                            (!gSet->ui()->autoTranslate() && snv->m_requestAutotranslate);

    if (xorAutotranslate && !snv->m_onceTranslated && (isValidLoadedUrl() || snv->m_auxContentLoaded))
        snv->transButton->click();

    snv->m_pageLoaded = true;
    snv->m_requestAutotranslate = false;

    if (snv->tabWidget()->currentWidget()==snv) {
        snv->takeScreenshot();
        snv->txtBrowser->setFocus();
    }
}

void CSnNet::userNavigationRequest(const QUrl &url, int type, bool isMainFrame)
{
    Q_UNUSED(type)

    if (!isValidLoadedUrl(url)) return;

    snv->m_onceTranslated = false;
    snv->m_fileChanged = false;
    snv->m_auxContentLoaded=false;

    if (isMainFrame) {
        CUrlHolder uh(QStringLiteral("(blank)"),url);
        gSet->appendMainHistory(uh);
    }
}

void CSnNet::processPixivNovel(const QUrl &url, const QString& title, bool translate, bool focus)
{
    auto ex = new CPixivNovelExtractor();
    ex->setParams(snv,title,translate,focus);
    auto th = new QThread();
    ex->moveToThread(th);
    th->start();

    // TODO here some leaks too
    auto rpl = gSet->auxNetworkAccessManager()->get(QNetworkRequest(url));
    gSet->app()->setOverrideCursor(Qt::BusyCursor);

    connect(rpl,qOverload<QNetworkReply::NetworkError>(&QNetworkReply::error),
            ex,&CPixivNovelExtractor::novelLoadError,Qt::QueuedConnection);
    connect(rpl,&QNetworkReply::finished,ex,&CPixivNovelExtractor::novelLoadFinished,Qt::QueuedConnection);
    connect(ex,&CPixivNovelExtractor::novelReady,this,&CSnNet::pixivNovelReady,Qt::QueuedConnection);
}

void CSnNet::downloadPixivManga()
{
    auto ac = qobject_cast<QAction*>(sender());
    if (!ac) return;
    QUrl origin = ac->data().toUrl();
    if (!origin.isValid()) return;

    snv->txtBrowser->page()->toHtml([this,origin](const QString& html){
        QStringList sl = CPixivNovelExtractor::parseJsonIllustPage(html,origin);
        if (sl.isEmpty()) // no JSON found
            sl = CPixivNovelExtractor::parseIllustPage(html);
        if (sl.isEmpty()) {
            QMessageBox::warning(snv,tr("JPReader"),
                                 tr("No pixiv manga image urls found"));
        } else {
            snv->netHandler->multiImgDownload(sl,origin);
        }
    });
}

void CSnNet::pixivNovelReady(const QString &html, bool focus, bool translate)
{
    auto sv = new CSnippetViewer(snv->parentWnd(),QUrl(),QStringList(),focus,html);
    sv->m_requestAutotranslate = translate;
}

void CSnNet::pdfConverted(const QString &html)
{
    if (html.isEmpty()) {
        snv->txtBrowser->setHtml(CGenericFuncs::makeSimpleHtml(QStringLiteral("PDF conversion error"),
                                                               QStringLiteral("Empty document.")));
    }
    if (html.length()<CDefaults::maxDataUrlFileSize) { // Small PDF files
        snv->txtBrowser->setHtml(html);
        snv->m_auxContentLoaded=false;
    } else { // Big PDF files
        loadWithTempFile(html,false);
    }
}

void CSnNet::pdfError(const QString &message)
{
    QString cn=CGenericFuncs::makeSimpleHtml(tr("Error"),tr("Unable to open PDF file.<br>%1").arg(message));
    snv->txtBrowser->setHtml(cn);
    QMessageBox::critical(snv,tr("JPReader"),tr("Unable to open PDF file."));
}

void CSnNet::load(const QUrl &url)
{
    if (gSet->isUrlBlocked(url)) {
        qInfo() << "adblock - skipping" << url;
        return;
    }

    snv->updateWebViewAttributes();
    if (snv->m_startPage)
        snv->m_startPage = false;

    QString fname = url.toLocalFile();
    if (!fname.isEmpty()) {
        QFileInfo fi(fname);
        if (!fi.exists() || !fi.isReadable()) {
            QString cn=CGenericFuncs::makeSimpleHtml(tr("Error"),tr("Unable to find file '%1'.").arg(fname));
            snv->m_fileChanged = false;
            snv->m_translationBkgdFinished=false;
            snv->m_loadingBkgdFinished=false;
            snv->txtBrowser->setHtml(cn,url);
            QMessageBox::critical(snv,tr("JPReader"),tr("Unable to open file."));
            return;
        }
        QString MIME = CGenericFuncs::detectMIME(fname);
        if (MIME.startsWith(QStringLiteral("text/plain"),Qt::CaseInsensitive) &&
                fi.size()<CDefaults::maxDataUrlFileSize) { // for small local txt files (load big files via file url directly)
            QFile data(fname);
            if (data.open(QFile::ReadOnly)) {
                QByteArray ba = data.readAll();
                QTextCodec* cd = CGenericFuncs::detectEncoding(ba);
                QString cn=CGenericFuncs::makeSimpleHtml(fi.fileName(),cd->toUnicode(ba));
                snv->m_fileChanged = false;
                snv->m_translationBkgdFinished=false;
                snv->m_loadingBkgdFinished=false;
                snv->txtBrowser->setHtml(cn,url);
                snv->m_auxContentLoaded=false;
                data.close();
            }
        } else if (MIME.startsWith(QStringLiteral("application/pdf"),Qt::CaseInsensitive)) {
            // for local pdf files
            snv->m_fileChanged = false;
            snv->m_translationBkgdFinished=false;
            snv->m_loadingBkgdFinished=false;
            auto pdf = new CPDFWorker();
            connect(this,&CSnNet::startPdfConversion,pdf,&CPDFWorker::pdfToText,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::gotText,this,&CSnNet::pdfConverted,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::error,this,&CSnNet::pdfError,Qt::QueuedConnection);
            auto pdft = new QThread();
            connect(pdf,&CPDFWorker::finished,pdft,&QThread::quit);
            connect(pdft,&QThread::finished,pdf,&CPDFWorker::deleteLater);
            connect(pdft,&QThread::finished,pdft,&QThread::deleteLater);
            pdf->moveToThread(pdft);
            pdft->start();
            Q_EMIT startPdfConversion(url.toString());
        } else { // for local html files
            snv->m_fileChanged = false;
            snv->txtBrowser->load(url);
            snv->m_auxContentLoaded=false;
        }
        gSet->appendRecent(fname);
    } else {
        QUrl u = url;
        CGenericFuncs::checkAndUnpackUrl(u);
        snv->m_fileChanged = false;
        snv->txtBrowser->load(u);
        snv->m_auxContentLoaded=false;
    }
    m_loadedUrl=url;
}

void CSnNet::load(const QString &html, const QUrl &baseUrl)
{
    snv->updateWebViewAttributes();
    snv->txtBrowser->setHtml(html,baseUrl);
    m_loadedUrl.clear();
    snv->m_auxContentLoaded=true;
}

void CSnNet::authenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator)
{
    CAuthDlg *dlg = new CAuthDlg(QApplication::activeWindow(),requestUrl,authenticator->realm());
    if (dlg->exec() == QDialog::Accepted) {
        authenticator->setUser(dlg->getUser());
        authenticator->setPassword(dlg->getPassword());
    } else {
        *authenticator = QAuthenticator();
    }
    dlg->setParent(nullptr);
    delete dlg;
}

void CSnNet::proxyAuthenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator,
                                         const QString &proxyHost)
{
    Q_UNUSED(proxyHost)
    CAuthDlg *dlg = new CAuthDlg(QApplication::activeWindow(),requestUrl,authenticator->realm());
    if (dlg->exec() == QDialog::Accepted) {
        authenticator->setUser(dlg->getUser());
        authenticator->setPassword(dlg->getPassword());
    } else {
        *authenticator = QAuthenticator();
    }
    dlg->setParent(nullptr);
    delete dlg;
}
