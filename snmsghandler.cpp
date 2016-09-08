#include <QMessageBox>
#include <QDebug>
#include "snmsghandler.h"

CSnMsgHandler::CSnMsgHandler(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    zoomFactor = 1.0;
    loadingBarHideTimer = new QTimer(this);
    loadingBarHideTimer->setInterval(1500);
    loadingBarHideTimer->setSingleShot(true);
    focusTimer = new QTimer(this);
    focusTimer->setInterval(1000);
    focusTimer->setSingleShot(true);

    connect(focusTimer,&QTimer::timeout,this,&CSnMsgHandler::focusSet);
}

void CSnMsgHandler::updateZoomFactor()
{
    snv->txtBrowser->page()->setZoomFactor(zoomFactor);
}

void CSnMsgHandler::searchFwd()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;
    if (snv->searchEdit->findText(snv->searchEdit->currentText(),
                                  Qt::MatchExactly)<0)
        snv->searchEdit->addItem(snv->searchEdit->currentText());
    snv->txtBrowser->findText(snv->searchEdit->currentText(), 0);
}

void CSnMsgHandler::searchBack()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;
    if (snv->searchEdit->findText(snv->searchEdit->currentText(),
                                  Qt::MatchExactly)<0)
        snv->searchEdit->addItem(snv->searchEdit->currentText());
    snv->txtBrowser->findText(snv->searchEdit->currentText(), QWebEnginePage::FindBackward);
}

void CSnMsgHandler::setZoom(QString z)
{
    bool okconv;
    int i = z.remove(QRegExp("[^0-9]")).toInt(&okconv);
    if (okconv)
        zoomFactor = static_cast<double>(i)/100.0;
    updateZoomFactor();
}

void CSnMsgHandler::navByClick()
{
    if (snv->urlEdit->text().isEmpty()) return;
    snv->navByUrl(snv->urlEdit->text());
}

void CSnMsgHandler::srcLang(int lang)
{
    if (!lockSrcLang.tryLock()) return;

    foreach (QAction *ac, gSet->ui.sourceLanguage->actions()) {
        bool okconv;
        int id = ac->data().toInt(&okconv);
        if (okconv && id>=0 && id<LSCOUNT && id==lang) {
            ac->setChecked(true);
            ac->trigger();
            break;
        }
    }

    lockSrcLang.unlock();
}

void CSnMsgHandler::tranEngine(int engine)
{
    if (!lockTranEngine.tryLock()) return;

    if (engine>=0 && engine<TECOUNT)
        gSet->settings.setTranslationEngine(engine);

    lockTranEngine.unlock();
}

void CSnMsgHandler::updateSrcLang(QAction *action)
{
    if (!lockSrcLang.tryLock()) return;

    bool okconv;
    int id = action->data().toInt(&okconv);
    if (okconv && id>=0 && id<LSCOUNT) {
        snv->comboSrcLang->setCurrentIndex(id);
    }

    lockSrcLang.unlock();
}

void CSnMsgHandler::updateTranEngine()
{
    if (!lockTranEngine.tryLock()) return;

    if (gSet->settings.translatorEngine>=0
            && gSet->settings.translatorEngine<snv->comboTranEngine->count())
        snv->comboTranEngine->setCurrentIndex(gSet->settings.translatorEngine);

    lockTranEngine.unlock();
}

void CSnMsgHandler::hideBarLoading()
{
    loadingBarHideTimer->start();
}

void CSnMsgHandler::focusSet()
{
    if (snv->urlEdit->text().isEmpty())
        snv->urlEdit->setFocus();
    else
        snv->txtBrowser->setFocus();
}

void CSnMsgHandler::renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus,
                                            int exitCode)
{
    if (terminationStatus!=QWebEnginePage::NormalTerminationStatus) {
        QString status = tr("Unknown");
        status.clear();
        switch (terminationStatus) {
            case QWebEnginePage::NormalTerminationStatus:
                status = tr("Normal exit");
                break;
            case QWebEnginePage::AbnormalTerminationStatus:
                status = tr("Abnormal exit");
                break;
            case QWebEnginePage::CrashedTerminationStatus:
                status = tr("Crashed");
                break;
            case QWebEnginePage::KilledTerminationStatus:
                status = tr("Killed");
                break;
        }
        status = tr("Render process exited abnormally.\nExit status: %1, code: %2")
                 .arg(status).arg(exitCode);
        qCritical() << status;
        QMessageBox::warning(snv,tr("JPReader"),status);
    }
}

void CSnMsgHandler::linkHovered(const QString &link)
{
    snv->statusBarMsg(link);
}
