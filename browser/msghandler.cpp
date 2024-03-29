#include <QMessageBox>
#include <QRegularExpression>
#include <QDebug>
#include "browser.h"
#include "msghandler.h"
#include "global/control.h"
#include "global/browserfuncs.h"
#include "global/network.h"
#include "global/actions.h"

namespace CDefaults {
const int focusTimerDelay = 1000;
const int loadingBarDelay = 1500;
}

CBrowserMsgHandler::CBrowserMsgHandler(CBrowserTab *parent)
    : QObject(parent),
      snv(parent)
{
    m_loadingBarHideTimer.setInterval(CDefaults::loadingBarDelay);
    m_loadingBarHideTimer.setSingleShot(true);
    m_focusTimer.setInterval(CDefaults::focusTimerDelay);
    m_focusTimer.setSingleShot(true);

    connect(&m_focusTimer,&QTimer::timeout,this,&CBrowserMsgHandler::urlEditSetFocus);

    connect(&m_loadingBarHideTimer,&QTimer::timeout,this,[this](){
        Q_EMIT loadingBarHide();
    });
}

void CBrowserMsgHandler::updateZoomFactor()
{
    snv->txtBrowser->page()->setZoomFactor(m_zoomFactor);
}

void CBrowserMsgHandler::activateFocusDelay()
{
    m_focusTimer.start();
}

void CBrowserMsgHandler::searchFwd()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;

    if (snv->searchEdit->findText(snv->searchEdit->currentText(),
                                  Qt::MatchExactly)<0) {
        snv->searchEdit->addItem(snv->searchEdit->currentText());
    }
    snv->txtBrowser->findText(snv->searchEdit->currentText());
    snv->txtBrowser->setFocus();
}

void CBrowserMsgHandler::searchBack()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;

    if (snv->searchEdit->findText(snv->searchEdit->currentText(),
                                  Qt::MatchExactly)<0) {
        snv->searchEdit->addItem(snv->searchEdit->currentText());
    }
    snv->txtBrowser->findText(snv->searchEdit->currentText(), QWebEnginePage::FindBackward);
    snv->txtBrowser->setFocus();
}

void CBrowserMsgHandler::searchFocus()
{
    snv->searchPanel->show();
    snv->searchEdit->setFocus();
}

void CBrowserMsgHandler::pastePassword(const QString& realm, CBrowserMsgHandler::PasteLoginMode mode)
{
    const QChar tabKey(0x9);
    QString user;
    QString pass;

    if (!gSet->browser()->haveSavedPassword(snv->txtBrowser->page()->url(),realm)) return;

    gSet->browser()->readPassword(snv->txtBrowser->page()->url(),realm,user,pass);
    QString inp = QSL("%1%2%3").arg(user,tabKey,pass);

    if (mode == PasteLoginMode::plmUsername) {
        inp = user;
    } else if (mode == PasteLoginMode::plmPassword) {
        inp = pass;
    }

    if (!inp.isEmpty())
        snv->sendInputToBrowser(inp);
}

void CBrowserMsgHandler::setZoom(const QString& z)
{
    static const QRegularExpression nonDigits(QSL("[^0-9]"));
    bool okconv = false;
    QString s = z;
    s.remove(nonDigits);
    int i = s.toInt(&okconv);
    if (okconv)
        m_zoomFactor = static_cast<double>(i)/100.0;
    updateZoomFactor();
}

void CBrowserMsgHandler::navByClick()
{
    if (snv->urlEdit->text().isEmpty()) return;
    snv->navByUrl(snv->urlEdit->text());
}

void CBrowserMsgHandler::tranEngine(int index)
{
    if (!m_lockTranEngine.tryLock()) return;

    QVariant v = snv->comboTranEngine->itemData(index);
    bool ok = false;
    int engine = v.toInt(&ok);
    if (ok) {
        auto e = static_cast<CStructures::TranslationEngine>(engine);
        if (CStructures::translationEngines().contains(e))
            gSet->net()->setTranslationEngine(e);
    }

    m_lockTranEngine.unlock();
}

void CBrowserMsgHandler::updateTranEngine()
{
    if (!m_lockTranEngine.tryLock()) return;

    int idx = snv->comboTranEngine->findData(gSet->settings()->translatorEngine);
    if (idx>=0)
        snv->comboTranEngine->setCurrentIndex(idx);

    m_lockTranEngine.unlock();
}

void CBrowserMsgHandler::hideBarLoading()
{
    m_loadingBarHideTimer.start();
}

void CBrowserMsgHandler::urlEditSetFocus()
{
    if (snv->urlEdit->text().isEmpty()) {
        snv->urlEdit->setFocus();
    } else {
        snv->txtBrowser->setFocus();
    }
}

void CBrowserMsgHandler::languageContextMenu(const QPoint &pos)
{
    QMenu cm;
    cm.addActions(gSet->actions()->getTranslationLanguagesActions());
    cm.exec(snv->comboTranEngine->mapToGlobal(pos));
}

void CBrowserMsgHandler::renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus,
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

void CBrowserMsgHandler::linkHovered(const QString &link)
{
    snv->statusBarMsg(link);
}

void CBrowserMsgHandler::showInspector()
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
