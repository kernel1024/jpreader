#include <QDebug>
#include <QStringList>
#include <QApplication>
#include "titlestranslator.h"
#include "globalcontrol.h"
#include "abstracttranslator.h"

CTitlesTranslator::CTitlesTranslator(QObject *parent) :
    QObject(parent)
{

}

void CTitlesTranslator::stop()
{
    stopReq = true;
}

void CTitlesTranslator::translateTitles(const QStringList &titles)
{
    QStringList res;
    stopReq=false;

    CAbstractTranslator* tran=CAbstractTranslator::translatorFactory(this, CLangPair(gSet->ui()->getActiveLangPair()));
    if (tran==nullptr || !tran->initTran()) {
        qCritical() << tr("Unable to initialize translation engine.");
        res.clear();
        res << QSL("ERROR");
        Q_EMIT gotTranslation(res);
        if (tran)
            tran->deleteLater();
        return;
    }
    int p = -1;
    for (int i=0;i<titles.count();i++) {
        if (stopReq) {
            res.clear();
            break;
        }
        QString s = tran->tranString(titles.at(i));
        if (s.contains(QSL("ERROR"))) break;
        res << s;
        if (100*i/titles.count()!=p) {
            Q_EMIT updateProgress(100*i/titles.count());
            p = 100*i/titles.count();
            QApplication::processEvents();
        }
    }
    tran->doneTran();
    tran->deleteLater();
    Q_EMIT updateProgress(-1);
    Q_EMIT gotTranslation(res);
}
