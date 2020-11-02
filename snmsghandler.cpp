#include <QMessageBox>
#include <QRegularExpression>
#include <QDebug>
#include "snviewer.h"
#include "snmsghandler.h"
#include "globalcontrol.h"
#include "genericfuncs.h"

namespace CDefaults {
const int focusTimerDelay = 1000;
const int loadingBarDelay = 1500;
}

CSnMsgHandler::CSnMsgHandler(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    m_zoomFactor = 1.0;
    m_loadingBarHideTimer.setInterval(CDefaults::loadingBarDelay);
    m_loadingBarHideTimer.setSingleShot(true);
    m_focusTimer.setInterval(CDefaults::focusTimerDelay);
    m_focusTimer.setSingleShot(true);

    connect(&m_focusTimer,&QTimer::timeout,this,&CSnMsgHandler::urlEditSetFocus);

    connect(&m_loadingBarHideTimer,&QTimer::timeout,this,[this](){
        Q_EMIT loadingBarHide();
    });
}

void CSnMsgHandler::updateZoomFactor()
{
    snv->txtBrowser->page()->setZoomFactor(m_zoomFactor);
}

void CSnMsgHandler::activateFocusDelay()
{
    m_focusTimer.start();
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
    QString inp = QSL("%1%2%3").arg(user,tabKey,pass);

    auto *ac = qobject_cast<QAction *>(sender());
    if (ac) {
        bool ok = false;
        int idx = ac->data().toInt(&ok);
        if (ok) {
            if (idx==2) inp = user;
            else if (idx==3) inp = pass;
        }
    }
    if (!inp.isEmpty())
        CGenericFuncs::sendKeyboardInputToView(snv->txtBrowser->page()->view(),inp);
}

void CSnMsgHandler::setZoom(const QString& z)
{
    bool okconv = false;
    QString s = z;
    s.remove(QRegularExpression(QSL("[^0-9]")));
    int i = s.toInt(&okconv);
    if (okconv)
        m_zoomFactor = static_cast<double>(i)/100.0;
    updateZoomFactor();
}

void CSnMsgHandler::navByClick()
{
    if (snv->urlEdit->text().isEmpty()) return;
    snv->navByUrl(snv->urlEdit->text());
}

void CSnMsgHandler::tranEngine(int index)
{
    if (!m_lockTranEngine.tryLock()) return;

    QVariant v = snv->comboTranEngine->itemData(index);
    bool ok = false;
    int engine = v.toInt(&ok);
    if (ok) {
        auto e = static_cast<CStructures::TranslationEngine>(engine);
        if (CStructures::translationEngines().contains(e))
            gSet->setTranslationEngine(e);
    }

    m_lockTranEngine.unlock();
}

void CSnMsgHandler::updateTranEngine()
{
    if (!m_lockTranEngine.tryLock()) return;

    int idx = snv->comboTranEngine->findData(gSet->settings()->translatorEngine);
    if (idx>=0)
        snv->comboTranEngine->setCurrentIndex(idx);

    m_lockTranEngine.unlock();
}

void CSnMsgHandler::hideBarLoading()
{
    m_loadingBarHideTimer.start();
}

void CSnMsgHandler::urlEditSetFocus()
{
    if (snv->urlEdit->text().isEmpty()) {
        snv->urlEdit->setFocus();
    } else {
        snv->txtBrowser->setFocus();
    }
}

void CSnMsgHandler::languageContextMenu(const QPoint &pos)
{
    QMenu cm;
    cm.addActions(gSet->getTranslationLanguagesActions());
    cm.exec(snv->comboTranEngine->mapToGlobal(pos));
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
        QMessageBox::warning(snv,QGuiApplication::applicationDisplayName(),status);
    }
}

void CSnMsgHandler::linkHovered(const QString &link)
{
    snv->statusBarMsg(link);
}

void CSnMsgHandler::showInspector()
{
    QList<int> heights;
    if (snv->splitter->sizes().constLast()>0) {
        heights << snv->splitter->height();
        heights << 0;
    } else {
        heights << 3*snv->splitter->height()/4;
        heights << snv->splitter->height()/4;
        snv->inspector->page()->setInspectedPage(snv->txtBrowser->page());
    }
    snv->splitter->setSizes(heights);
}
