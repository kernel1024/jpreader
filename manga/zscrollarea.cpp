#include <QResizeEvent>
#include "zscrollarea.h"
#include "global/control.h"
#include "global/ui.h"

namespace ZDefaults {
const int resizeTimerInitialMS = 500;
const int resizeTimerDiffMS = 200;
}

ZScrollArea::ZScrollArea(QWidget *parent) :
    QScrollArea(parent)
{
    resizeTimer.setInterval(ZDefaults::resizeTimerInitialMS);
    resizeTimer.setSingleShot(true);
    connect(&resizeTimer,&QTimer::timeout,this,[this](){
        if (savedSize.isValid())
            Q_EMIT sizeChanged(savedSize);
    });
}

void ZScrollArea::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    savedSize = event->size();

    int preferredInterval = static_cast<int>(gSet->ui()->getMangaAvgFineRenderTime()) / 2;
    if ((qAbs(preferredInterval - resizeTimer.interval()) > ZDefaults::resizeTimerDiffMS)
            && (preferredInterval > 100))
        resizeTimer.setInterval(preferredInterval);

    resizeTimer.start();
}
