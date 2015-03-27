#include <QDebug>
#include <QStringList>
#include <QApplication>
#include "titlestranslator.h"
#include "globalcontrol.h"
#include "abstracttranslator.h"

CTitlesTranslator::CTitlesTranslator(QObject *parent) :
    QObject(parent),
    inProgress(false),
    stopReq(false)
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

    CAbstractTranslator* tran=translatorFactory(this);
    if (tran==NULL || !tran->initTran()) {
        qDebug() << tr("Unable to initialize ATLAS.");
        res.clear();
        res << "ERROR";
        emit gotTranslation(res);
        if (tran!=NULL)
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
        if (s.contains("ERROR")) break;
        res << s;
        if (100*i/titles.count()!=p) {
            emit updateProgress(100*i/titles.count());
            p = 100*i/titles.count();
            QApplication::processEvents();
        }
    }
    tran->doneTran();
    tran->deleteLater();
    emit updateProgress(-1);
    emit gotTranslation(res);
}
