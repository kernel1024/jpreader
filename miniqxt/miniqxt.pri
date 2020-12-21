INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

PKGCONFIG += xcb xcb-keysyms xcb-xfixes xcb-image

HEADERS += \
    $$PWD/qxttooltip.h \
    $$PWD/qxttooltip_p.h \
    $$PWD/qxtglobalshortcut.h \
    $$PWD/xcbtools.h

SOURCES += \
    $$PWD/qxttooltip.cpp \
    $$PWD/qxtglobalshortcut.cpp \
    $$PWD/xcbtools.cpp
