#include <QMessageBox>
#include "snmsghandler.h"

CSnMsgHandler::CSnMsgHandler(CSnippetViewer *parent)
    : QObject(parent)
{
    snv = parent;
    loadingBarHideTimer = new QTimer(this);
    connect(loadingBarHideTimer,SIGNAL(timeout()),loadingBarHideTimer,SLOT(stop()));
}

void CSnMsgHandler::searchFwd()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;
    snv->txtBrowser->findText(snv->searchEdit->currentText(), 0);
}

void CSnMsgHandler::searchBack()
{
    if (snv->searchEdit->currentText().isEmpty()) return ;
    snv->txtBrowser->findText(snv->searchEdit->currentText(), QWebEnginePage::FindBackward);
}

void CSnMsgHandler::setZoom(QString z)
{
    if (snv->txtBrowser) {
        bool okconv;
        int i = z.remove(QRegExp("[^0-9]")).toInt(&okconv);
        if (okconv)
            snv->txtBrowser->setZoomFactor(((double)i)/100);
    }
}

void CSnMsgHandler::urlEdited(const QString &)
{
}

void CSnMsgHandler::navByClick()
{
    if (snv->urlEdit->currentText().isEmpty()) return;
    snv->navByUrl(snv->urlEdit->currentText());
}

void CSnMsgHandler::srcLang(int lang)
{
    if (!lockSrcLang.tryLock()) return;

    foreach (QAction *ac, gSet->sourceLanguage->actions()) {
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
        gSet->setTranslationEngine(engine);

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

    if (gSet->translatorEngine>=0 && gSet->translatorEngine<snv->comboTranEngine->count())
        snv->comboTranEngine->setCurrentIndex(gSet->translatorEngine);

    lockTranEngine.unlock();
}

void CSnMsgHandler::hideBarLoading()
{
    loadingBarHideTimer->start(1500);
}

void CSnMsgHandler::linkHovered(const QString &link)
{
    snv->statusBarMsg(link);
}
