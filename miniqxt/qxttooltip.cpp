#include "qxttooltip.h"
/****************************************************************************
** Copyright (c) 2006 - 2011, the LibQxt project.
** See the Qxt AUTHORS file for a list of authors and copyright holders.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the LibQxt project nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
** <http://libqxt.org>  <foundation@libqxt.org>
*****************************************************************************/

#include "qxttooltip_p.h"
#include <QStyleOptionFrame>
#include <QDesktopWidget>
#include <QStylePainter>
#include <QApplication>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QToolTip>
#include <QPalette>
#include <QTimer>
#include <QFrame>
#include <QStyle>
#include <QHash>

static const Qt::WindowFlags FLAGS = Qt::ToolTip;

QxtToolTipPrivate* QxtToolTipPrivate::self = 0;

QxtToolTipPrivate* QxtToolTipPrivate::instance()
{
    if (!self)
        self = new QxtToolTipPrivate();
    return self;
}

QxtToolTipPrivate::QxtToolTipPrivate() : QWidget(qApp->desktop(), FLAGS)
{
    ignoreEnterEvent = false;
    allowCloseOnLeave = false;
    setWindowFlags(FLAGS);
    vbox = new QVBoxLayout(this);
    setPalette(QToolTip::palette());
    setWindowOpacity(style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, 0, this) / 255.0);
    layout()->setMargin(style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, 0, this));
    qApp->installEventFilter(this);
}

QxtToolTipPrivate::~QxtToolTipPrivate()
{
    qApp->removeEventFilter(this); // not really necessary but rather for completeness :)
    self = 0;
}

void QxtToolTipPrivate::show(const QPoint& pos, QWidget* tooltip, QWidget* parent, const QRect& rect, const bool allowMouseEnter)
{
//    Q_ASSERT(tooltip && parent);
    if (!isVisible())
    {
        int scr = 0;
        if (QApplication::desktop()->isVirtualDesktop())
            scr = QApplication::desktop()->screenNumber(pos);
        else
            scr = QApplication::desktop()->screenNumber(this);
        setParent(QApplication::desktop()->screen(scr));
        setWindowFlags(FLAGS);
        setToolTip(tooltip);
        currentParent = parent;
        currentRect = rect;
        ignoreEnterEvent = allowMouseEnter;
        allowCloseOnLeave = false;
        move(calculatePos(scr, pos));
        QWidget::show();
    }
}

void QxtToolTipPrivate::setToolTip(QWidget* tooltip)
{
    for (int i = 0; i < vbox->count(); ++i)
    {
        QLayoutItem* item = layout()->takeAt(i);
        if (item->widget())
            item->widget()->hide();
    }
    vbox->addWidget(tooltip);
    tooltip->show();
}

void QxtToolTipPrivate::enterEvent(QEvent* event)
{
    Q_UNUSED(event);
    if (ignoreEnterEvent)
        allowCloseOnLeave = true;
    else
        hideLater();
}

void QxtToolTipPrivate::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (!ignoreEnterEvent || allowCloseOnLeave)
        hideLater();
}

void QxtToolTipPrivate::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QStylePainter painter(this);
    QStyleOptionFrame opt;
    opt.initFrom(this);
    painter.drawPrimitive(QStyle::PE_PanelTipLabel, opt);
}

bool QxtToolTipPrivate::eventFilter(QObject* object, QEvent* event)
{
    QKeyEvent* keyEvent;
    int key;
    Qt::KeyboardModifiers mods;
    QPoint pos;
    QWidget* widget;
    QHelpEvent* helpEvent;
    QRect area;

    switch (event->type())
    {
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            // accept only modifiers
            keyEvent = static_cast<QKeyEvent*>(event);
            key = keyEvent->key();
            mods = keyEvent->modifiers();
            if ((mods & Qt::KeyboardModifierMask) ||
                    (key == Qt::Key_Shift || key == Qt::Key_Control ||
                     key == Qt::Key_Alt || key == Qt::Key_Meta))
                break;
            hideLater();
            break;
        case QEvent::Leave:
            if (!ignoreEnterEvent)
                hideLater();
            break;
        case QEvent::WindowActivate:
        case QEvent::WindowDeactivate:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::FocusIn:
        case QEvent::FocusOut:
        case QEvent::Wheel:
            hideLater();
            break;
        case QEvent::MouseMove:
            pos = static_cast<QMouseEvent*>(event)->pos();
            if (!currentRect.isNull() && !currentRect.contains(pos))
                hideLater();
            break;
        case QEvent::ToolTip:
            // eat appropriate tooltip events
            widget = qobject_cast<QWidget*>(object);
            if (widget!=nullptr && tooltips.contains(widget))
            {
                helpEvent = static_cast<QHelpEvent*>(event);
                area = tooltips.value(widget).second;
                if (area.isNull() || area.contains(helpEvent->pos()))
                {
                    show(helpEvent->globalPos(), tooltips.value(widget).first, widget, area);
                    return true;
                }
            }
            break;
        default:
            break;
    }
    return false;
}

void QxtToolTipPrivate::hideLater()
{
    currentRect = QRect();
    if (isVisible())
        QTimer::singleShot(0, this, SLOT(hide()));
}

QPoint QxtToolTipPrivate::calculatePos(int scr, const QPoint& eventPos) const
{
#ifdef Q_WS_MAC
    QRect screen = QApplication::desktop()->availableGeometry(scr);
#else
    QRect screen = QApplication::desktop()->screenGeometry(scr);
#endif

    QPoint p = eventPos;
    p += QPoint(2,
#ifdef Q_WS_WIN
                24
#else
                16
#endif
               );
    QSize s = sizeHint();
    if (p.x() + s.width() > screen.x() + screen.width())
        p.rx() -= 4 + s.width();
    if (p.y() + s.height() > screen.y() + screen.height())
        p.ry() -= 24 + s.height();
    if (p.y() < screen.y())
        p.setY(screen.y());
    if (p.x() + s.width() > screen.x() + screen.width())
        p.setX(screen.x() + screen.width() - s.width());
    if (p.x() < screen.x())
        p.setX(screen.x());
    if (p.y() + s.height() > screen.y() + screen.height())
        p.setY(screen.y() + screen.height() - s.height());
    return p;
}

QxtToolTip::QxtToolTip()
{
}

void QxtToolTip::show(const QPoint& pos, QWidget* tooltip, QWidget* parent, const QRect& rect,
                      const bool allowMouseEnter)
{
    QxtToolTipPrivate::instance()->show(pos, tooltip, parent, rect, allowMouseEnter);
}

void QxtToolTip::hide()
{
    QxtToolTipPrivate::instance()->hide();
}

QWidget* QxtToolTip::toolTip(QWidget* parent)
{
    Q_ASSERT(parent);
    QWidget* tooltip = 0;
    if (!QxtToolTipPrivate::instance()->tooltips.contains(parent))
        qWarning("QxtToolTip::toolTip: Unknown parent");
    else
        tooltip = QxtToolTipPrivate::instance()->tooltips.value(parent).first;
    return tooltip;
}

void QxtToolTip::setToolTip(QWidget* parent, QWidget* tooltip, const QRect& rect)
{
    Q_ASSERT(parent);
    if (tooltip)
    {
        // set
        tooltip->hide();
        QxtToolTipPrivate::instance()->tooltips[parent] = qMakePair(QPointer<QWidget>(tooltip), rect);
    }
    else
    {
        // remove
        if (!QxtToolTipPrivate::instance()->tooltips.contains(parent))
            qWarning("QxtToolTip::setToolTip: Unknown parent");
        else
            QxtToolTipPrivate::instance()->tooltips.remove(parent);
    }
}

QRect QxtToolTip::toolTipRect(QWidget* parent)
{
    Q_ASSERT(parent);
    QRect rect;
    if (!QxtToolTipPrivate::instance()->tooltips.contains(parent))
        qWarning("QxtToolTip::toolTipRect: Unknown parent");
    else
        rect = QxtToolTipPrivate::instance()->tooltips.value(parent).second;
    return rect;
}

void QxtToolTip::setToolTipRect(QWidget* parent, const QRect& rect)
{
    Q_ASSERT(parent);
    if (!QxtToolTipPrivate::instance()->tooltips.contains(parent))
        qWarning("QxtToolTip::setToolTipRect: Unknown parent");
    else
        QxtToolTipPrivate::instance()->tooltips[parent].second = rect;
}

int QxtToolTip::margin()
{
    return QxtToolTipPrivate::instance()->layout()->margin();
}

void QxtToolTip::setMargin(int margin)
{
    QxtToolTipPrivate::instance()->layout()->setMargin(margin);
}

qreal QxtToolTip::opacity()
{
    return QxtToolTipPrivate::instance()->windowOpacity();
}

void QxtToolTip::setOpacity(qreal level)
{
    QxtToolTipPrivate::instance()->setWindowOpacity(level);
}
