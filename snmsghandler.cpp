#include <QMessageBox>
#include <QDebug>
#include "snviewer.h"
#include "snmsghandler.h"
#include "globalcontrol.h"
#include "genericfuncs.h"

const int focusTimerDelay = 1000;
const int loadingBarDelay = 1500;

CSnMsgHandler::CSnMsgHandler(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    zoomFactor = 1.0;
    loadingBarHideTimer = new QTimer(this);
    loadingBarHideTimer->setInterval(loadingBarDelay);
    loadingBarHideTimer->setSingleShot(true);
    focusTimer = new QTimer(this);
    focusTimer->setInterval(focusTimerDelay);
    focusTimer->setSingleShot(true);

    connect(focusTimer,&QTimer::timeout,this,&CSnMsgHandler::urlEditSetFocus);

    connect(loadingBarHideTimer,&QTimer::timeout,this,[this](){
        Q_EMIT loadingBarHide();
    });
}

void CSnMsgHandler::updateZoomFactor()
{
    snv->txtBrowser->page()->setZoomFactor(zoomFactor);
}

void CSnMsgHandler::activateFocusDelay()
{
    focusTimer->start();
}

void CSnMsgHandler::searchFwd()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;

    if (snv->searchEdit->findText(snv->searchEdit->currentText(),
                                  Qt::MatchExactly)<0) {
        snv->searchEdit->addItem(snv->searchEdit->currentText());
    }
    snv->txtBrowser->findText(snv->searchEdit->currentText());
    snv->txtBrowser->setFocus();
}

void CSnMsgHandler::searchBack()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;

    if (snv->searchEdit->findText(snv->searchEdit->currentText(),
                                  Qt::MatchExactly)<0) {
        snv->searchEdit->addItem(snv->searchEdit->currentText());
    }
    snv->txtBrowser->findText(snv->searchEdit->currentText(), QWebEnginePage::FindBackward);
    snv->txtBrowser->setFocus();
}

void CSnMsgHandler::searchFocus()
{
    snv->searchPanel->show();
    snv->searchEdit->setFocus();
}

void CSnMsgHandler::pastePassword()
{
    const QChar tabKey(0x9);
    QString user;
    QString pass;

    if (!gSet->haveSavedPassword(snv->txtBrowser->page()->url())) return;

    gSet->readPassword(snv->txtBrowser->page()->url(),user,pass);
    QString inp = QStringLiteral("%1%2%3").arg(user,tabKey,pass);

    auto ac = qobject_cast<QAction *>(sender());
    if (ac) {
        bool ok;
        int idx = ac->data().toInt(&ok);
        if (ok) {
            if (idx==2) inp = user;
            else if (idx==3) inp = pass;
        }
    }
    if (!inp.isEmpty())
        sendKeyboardInputToView(snv->txtBrowser->page()->view(),inp);
}

void CSnMsgHandler::setZoom(const QString& z)
{
    bool okconv;
    QString s = z;
    s.remove(QRegExp(QStringLiteral("[^0-9]")));
    int i = s.toInt(&okconv);
    if (okconv)
        zoomFactor = static_cast<double>(i)/100.0;
    updateZoomFactor();
}

void CSnMsgHandler::navByClick()
{
    if (snv->urlEdit->text().isEmpty()) return;
    snv->navByUrl(snv->urlEdit->text());
}

void CSnMsgHandler::tranEngine(int index)
{
    if (!lockTranEngine.tryLock()) return;

    QVariant v = snv->comboTranEngine->itemData(index);
    bool ok;
    int engine = v.toInt(&ok);
    if (ok) {
        auto e = static_cast<TranslationEngine>(engine);
        if (translationEngines().contains(e))
            gSet->settings.setTranslationEngine(e);
    }

    lockTranEngine.unlock();
}

void CSnMsgHandler::updateTranEngine()
{
    if (!lockTranEngine.tryLock()) return;

    int idx = snv->comboTranEngine->findData(gSet->settings.translatorEngine);
    if (idx>=0)
        snv->comboTranEngine->setCurrentIndex(idx);

    lockTranEngine.unlock();
}

void CSnMsgHandler::hideBarLoading()
{
    loadingBarHideTimer->start();
}

void CSnMsgHandler::urlEditSetFocus()
{
    if (snv->urlEdit->text().isEmpty()) {
        snv->urlEdit->setFocus();
    } else {
        snv->txtBrowser->setFocus();
    }
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
