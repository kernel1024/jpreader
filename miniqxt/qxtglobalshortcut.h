#ifndef QXTGLOBALSHORTCUT_H
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

#define QXTGLOBALSHORTCUT_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <xcb/xcb.h>

class QxtGlobalShortcut : public QObject,
        public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit QxtGlobalShortcut(QObject* parent = 0);
    explicit QxtGlobalShortcut(const QKeySequence& shortcut, QObject* parent = 0);
    virtual ~QxtGlobalShortcut();

    QKeySequence shortcut() const;
    bool setShortcut(const QKeySequence& shortcut);
    bool unsetShortcut();

    bool isEnabled() const;

public slots:
    void setEnabled(bool enabled = true);
    void setDisabled(bool disabled = true);

signals:
    void activated();

private:

    bool m_enabled;
    Qt::Key key;
    Qt::KeyboardModifiers mods;

    static bool error;
    static int ref;
    virtual bool nativeEventFilter(const QByteArray & eventType, void * message, long * result);

    static void activateShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods);

private:
    static xcb_keycode_t nativeKeycode(Qt::Key keycode, Qt::KeyboardModifiers modifiers);
    static uint16_t nativeModifiers(Qt::KeyboardModifiers modifiers);

    static bool registerShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods);
    static bool unregisterShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods);

    static QHash<QPair<xcb_keycode_t, uint16_t>, QxtGlobalShortcut*> shortcuts;

};

#endif // QXTGLOBALSHORTCUT_H
