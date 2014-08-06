#include <QDebug>
#include <QStringList>
#include <QApplication>
#include "titlestranslator.h"
#include "atlastranslator.h"
#include "globalcontrol.h"

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

    CAtlasTranslator atlas;
    if (!atlas.initTran(gSet->atlHost,gSet->atlPort)) {
        qDebug() << tr("Unable to initialize ATLAS.");
        res.clear();
        res << "ERROR";
        emit gotTranslation(res);
        return;
    }
    int p = -1;
    for (int i=0;i<titles.count();i++) {
        if (stopReq) {
            res.clear();
            break;
        }
        QString s = atlas.tranString(titles.at(i));
        if (s.contains("ERROR")) break;
        res << s;
        if (100*i/titles.count()!=p) {
            emit updateProgress(100*i/titles.count());
            p = 100*i/titles.count();
            QApplication::processEvents();
        }
    }
    atlas.doneTran();
    emit updateProgress(-1);
    emit gotTranslation(res);
}
