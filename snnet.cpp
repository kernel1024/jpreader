#include <QTextCodec>
#include <QPointer>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrlQuery>
#include <QWebEngineScriptCollection>
#include <QDirIterator>
#include <QRegExp>
#include <QDialog>

#include "snnet.h"
#include "snmsghandler.h"
#include "genericfuncs.h"
#include "authdlg.h"
#include "pdftotext.h"
#include "downloadmanager.h"
#include "genericfuncs.h"
#include "ui_selectablelistdlg.h"

CSnNet::CSnNet(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    loadedUrl.clear();
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
        QString dir = getExistingDirectoryD(snv,tr("Save images to directory"),getTmpDir());
        int index = 0;
        for (const QListWidgetItem* itm : ui.list->selectedItems()){
            if (!ui.checkAddNumbers->isChecked()) {
                index = -1;
            } else {
                index++;
            }

            gSet->downloadManager->handleAuxDownload(itm->text(),dir,referer,index,ui.list->selectedItems().count());
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
    if (url.scheme().startsWith("http",Qt::CaseInsensitive)) return true;
    return false;
}

bool CSnNet::isValidLoadedUrl()
{
    return isValidLoadedUrl(loadedUrl);
}

bool CSnNet::loadWithTempFile(const QString &html, bool createNewTab)
{
    QByteArray ba = html.toUtf8();
    QString fname = gSet->makeTmpFileName("html",true);
    QFile f(fname);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(ba);
        f.close();
        gSet->createdFiles.append(fname);
        if (createNewTab)
            new CSnippetViewer(snv->parentWnd,QUrl::fromLocalFile(fname));
        else {
            snv->fileChanged = false;
            snv->txtBrowser->load(QUrl::fromLocalFile(fname));
            snv->auxContentLoaded=false;
        }
        return true;
    } else
        QMessageBox::warning(snv,tr("JPReader"),tr("Unable to create temporary file "
                                                   "for document."));
    return false;
}

void CSnNet::loadStarted()
{
    snv->barLoading->setValue(0);
    snv->barLoading->show();
    snv->barPlaceholder->hide();
    snv->loading = true;
    snv->updateButtonsState();
}

void CSnNet::loadFinished(bool ok)
{
    snv->msgHandler->updateZoomFactor();
    snv->msgHandler->hideBarLoading();
    snv->loading = false;
    snv->updateButtonsState();

    if (isValidLoadedUrl(snv->txtBrowser->url()) && ok) {
        snv->auxContentLoaded=false;
        loadedUrl = snv->txtBrowser->url();
    }

    if (snv->parentWnd->tabMain->currentIndex()!=snv->parentWnd->tabMain->indexOf(snv) &&
            !snv->translationBkgdFinished) { // tab is inactive
        snv->loadingBkgdFinished=true;
        snv->parentWnd->tabMain->tabBar()->setTabTextColor(snv->parentWnd->tabMain->indexOf(snv),Qt::blue);
        snv->parentWnd->updateTabs();
    }

    bool xorAutotranslate = (gSet->ui.autoTranslate() && !snv->requestAutotranslate) ||
                            (!gSet->ui.autoTranslate() && snv->requestAutotranslate);

    if (xorAutotranslate && !snv->onceTranslated && (isValidLoadedUrl() || snv->auxContentLoaded))
        snv->transButton->click();

    snv->pageLoaded = true;
    snv->requestAutotranslate = false;

    if (snv->tabWidget->currentWidget()==snv) {
        snv->takeScreenshot();
        snv->txtBrowser->setFocus();
    }
}

void CSnNet::userNavigationRequest(const QUrl &url, const int type, const bool isMainFrame)
{
    Q_UNUSED(type);

    if (!isValidLoadedUrl(url)) return;

    snv->onceTranslated = false;
    snv->fileChanged = false;
    snv->auxContentLoaded=false;

    if (isMainFrame) {
        UrlHolder uh(QString("(blank)"),url);
        gSet->appendMainHistory(uh);
    }
}

void CSnNet::processPixivNovel(const QUrl &url, const QString& title, bool translate, bool focus)
{
    QNetworkReply* rpl = gSet->auxNetManager->get(QNetworkRequest(url));
    qApp->setOverrideCursor(Qt::BusyCursor);

    connect(rpl,SIGNAL(error(QNetworkReply::NetworkError)),
            this,SLOT(pixivNovelError(QNetworkReply::NetworkError)));
    connect(rpl,&QNetworkReply::finished,[this,rpl,title,translate,focus](){
        qApp->restoreOverrideCursor();
        if (rpl==nullptr) return;

        if (rpl->error() == QNetworkReply::NoError) {
            QString html = QString::fromUtf8(rpl->readAll());

            QString wtitle = title;
            if (wtitle.isEmpty())
                wtitle = extractFileTitle(html);

            QString htitle;
            QRegExp hrx("<h1[^>]*class=\"title\"[^>]*>",Qt::CaseInsensitive);
            int hidx = hrx.indexIn(html);
            if (hidx>=0) {
                int hpos = hidx+hrx.matchedLength();
                int hlen = html.indexOf('<',hpos) - hpos;
                htitle = html.mid(hpos, hlen);
            }

            QString hauthor;
            QString hauthornum;
            QRegExp arx("<a[^>]*class=\"user-name\"[^>]*>");
            int aidx = arx.indexIn(html);
            if (aidx>=0) {
                QString acap = arx.cap();
                int anpos = acap.indexOf("id=")+3;
                int anlen = acap.indexOf("\"",anpos) - anpos;
                hauthornum = acap.mid(anpos,anlen);

                int apos = aidx+arx.matchedLength();
                int alen = html.indexOf('<',apos) - apos;
                hauthor = html.mid(apos, alen);
            }

            QStringList tags;
            QRegExp trx("<li[^>]*class=\"tag\"[^>]*>",Qt::CaseInsensitive);
            QRegExp ttrx("class=\"text\"[^>]*>",Qt::CaseInsensitive);
            int tpos = 0;
            int tstop = html.indexOf("template-work-tags",Qt::CaseInsensitive);
            int tidx = trx.indexIn(html,tpos);
            while (tidx>=0) {
                tpos = tidx + trx.matchedLength();
                int ttidx = ttrx.indexIn(html,tpos) + ttrx.matchedLength();
                int ttlen = html.indexOf('<',ttidx) - ttidx;
                if (ttidx>=0 && ttlen>=0 && ttidx<tstop) {
                    tags << html.mid(ttidx,ttlen);
                }
                tidx = trx.indexIn(html,tpos);
            }

            QRegExp rx("<textarea[^>]*id=\"novel_text\"[^>]*>",Qt::CaseInsensitive);
            int idx = rx.indexIn(html);
            if (idx<0) return;
            html.remove(0,idx+rx.matchedLength());
            idx = html.indexOf(QString("</textarea>"),0,Qt::CaseInsensitive);
            if (idx>=0)
                html.truncate(idx);

            QRegExp rbrx("\\[\\[rb\\:.*\\]\\]");
            rbrx.setMinimal(true);
            int pos = 0;
            while ((pos = rbrx.indexIn(html,pos)) != -1) {
                QString rb = rbrx.cap(0);
                rb.replace("&gt;", ">");
                rb.remove(QRegExp("^.*rb\\:"));
                rb.remove(QRegExp("\\>.*"));
                rb = rb.trimmed();
                if (!rb.isEmpty())
                    html.replace(pos,rbrx.matchedLength(),rb);
                else
                    pos += rbrx.matchedLength();
            }

            if (!tags.isEmpty())
                html.prepend(QString("Tags: %1\n\n").arg(tags.join(" / ")));
            if (!hauthor.isEmpty())
                html.prepend(QString("Author: <a href=\"https://www.pixiv.net/member.php?id=%1\">%2</a>\n\n")
                             .arg(hauthornum,hauthor));
            if (!htitle.isEmpty())
                html.prepend(QString("Title: <b>%1</b>\n\n").arg(htitle));

            CSnippetViewer* sv = new CSnippetViewer(snv->parentWnd,QUrl(),QStringList(),
                                                    focus,makeSimpleHtml(wtitle,html,true,rpl->url()));
            sv->requestAutotranslate = translate;
        }
        rpl->deleteLater();
    });
}

void CSnNet::pixivNovelError(QNetworkReply::NetworkError )
{
    QString msg("Unable to load pixiv novel.");
    QNetworkReply* rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl!=nullptr)
        msg.append(QString(" %1").arg(rpl->errorString()));
    QMessageBox::warning(snv->parentWnd,tr("JPReader"),msg);
}

void CSnNet::pdfConverted(const QString &html)
{
    if (html.isEmpty()) {
        snv->txtBrowser->setHtml(makeSimpleHtml("PDF conversion error","Empty document."));
    }
    if (html.length()<1024*1024) { // Small PDF files
        snv->txtBrowser->setHtml(html);
        snv->auxContentLoaded=false;
    } else { // Big PDF files
        loadWithTempFile(html,false);
    }
}

void CSnNet::pdfError(const QString &message)
{
    QString cn=makeSimpleHtml(tr("Error"),tr("Unable to open PDF file.<br>%1").arg(message));
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
    if (snv->isStartPage)
        snv->isStartPage = false;

    QString fname = url.toLocalFile();
    if (!fname.isEmpty()) {
        QFileInfo fi(fname);
        if (!fi.exists() || !fi.isReadable()) {
            QString cn=makeSimpleHtml(tr("Error"),tr("Unable to find file '%1'.").arg(fname));
            snv->fileChanged = false;
            snv->translationBkgdFinished=false;
            snv->loadingBkgdFinished=false;
            snv->txtBrowser->setHtml(cn,url);
            QMessageBox::critical(snv,tr("JPReader"),tr("Unable to open file."));
            return;
        }
        QString MIME = detectMIME(fname);
        if (MIME.startsWith("text/plain",Qt::CaseInsensitive) &&
                fi.size()<1024*1024) { // for small local txt files (load big files via file url directly)
            QFile data(fname);
            if (data.open(QFile::ReadOnly)) {
                QByteArray ba = data.readAll();
                QTextCodec* cd = detectEncoding(ba);
                QString cn=makeSimpleHtml(fi.fileName(),cd->toUnicode(ba));
                snv->fileChanged = false;
                snv->translationBkgdFinished=false;
                snv->loadingBkgdFinished=false;
                snv->txtBrowser->setHtml(cn,url);
                snv->auxContentLoaded=false;
                data.close();
            }
        } else if (MIME.startsWith("application/pdf",Qt::CaseInsensitive)) { // for local pdf files
            QString cn;
            snv->fileChanged = false;
            snv->translationBkgdFinished=false;
            snv->loadingBkgdFinished=false;
            CPDFWorker* pdf = new CPDFWorker();
            connect(this,&CSnNet::startPdfConversion,pdf,&CPDFWorker::pdfToText,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::gotText,this,&CSnNet::pdfConverted,Qt::QueuedConnection);
            connect(pdf,&CPDFWorker::error,this,&CSnNet::pdfError,Qt::QueuedConnection);
            QThread* pdft = new QThread();
            pdf->moveToThread(pdft);
            pdft->start();
            emit startPdfConversion(url.toString());
        } else { // for local html files
            snv->fileChanged = false;
            snv->txtBrowser->load(url);
            snv->auxContentLoaded=false;
        }
        gSet->appendRecent(fname);
    } else {
        QUrl u = url;
        checkAndUnpackUrl(u);
        snv->fileChanged = false;
        snv->txtBrowser->load(u);
        snv->auxContentLoaded=false;
    }
    loadedUrl=url;
}

void CSnNet::load(const QString &html, const QUrl &baseUrl)
{
    snv->updateWebViewAttributes();
    snv->txtBrowser->setHtml(html,baseUrl);
    loadedUrl.clear();
    snv->auxContentLoaded=true;
}

void CSnNet::authenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator)
{
    CAuthDlg *dlg = new CAuthDlg(QApplication::activeWindow(),requestUrl,authenticator->realm());
    if (dlg->exec()) {
        authenticator->setUser(dlg->getUser());
        authenticator->setPassword(dlg->getPassword());
    } else
        *authenticator = QAuthenticator();
    dlg->setParent(nullptr);
    delete dlg;
}

void CSnNet::proxyAuthenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator,
                                         const QString &proxyHost)
{
    Q_UNUSED(proxyHost);
    CAuthDlg *dlg = new CAuthDlg(QApplication::activeWindow(),requestUrl,authenticator->realm());
    if (dlg->exec()) {
        authenticator->setUser(dlg->getUser());
        authenticator->setPassword(dlg->getPassword());
    } else
        *authenticator = QAuthenticator();
    dlg->setParent(nullptr);
    delete dlg;
}
