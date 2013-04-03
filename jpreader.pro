SUBDIRS += qtsingleapplication

HEADERS = mainwindow.h \
    snviewer.h \
    settingsdlg.h \
    waitdlg.h \
    calcthread.h \
    specwidgets.h \
    atlastranslator.h \
    genericfuncs.h \
    globalcontrol.h \
    snmsghandler.h \
    snctxhandler.h \
    sntrans.h \
    snnet.h \
    authdlg.h \
    bookmarkdlg.h \
    searchtab.h \
    lighttranslator.h \
    auxtranslator.h \
    recollsearch.h \
    indexersearch.h

SOURCES = main.cpp \
    mainwindow.cpp \
    settingsdlg.cpp \
    snviewer.cpp \
    waitdlg.cpp \
    calcthread.cpp \
    atlastranslator.cpp \
    specwidgets.cpp \
    genericfuncs.cpp \
    globalcontrol.cpp \
    snmsghandler.cpp \
    snctxhandler.cpp \
    sntrans.cpp \
    snnet.cpp \
    authdlg.cpp \
    bookmarkdlg.cpp \
    searchtab.cpp \
    lighttranslator.cpp \
    auxtranslator.cpp \
    recollsearch.cpp \
    indexersearch.cpp

RESOURCES = \
    jpreader.qrc

FORMS = main.ui \
    settingsdlg.ui \
    waitdlg.ui \
    authdlg.ui \
    bookmarkdlg.ui \
    snviewer.ui \
    searchtab.ui \
    lighttranslator.ui

QT += webkit network xml dbus
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets webkitwidgets

DEFINES += WITHWEBKIT
CONFIG += warn_on link_pkgconfig

exists( /usr/include/X11/Xlib.h ) {
    LIBS += -lX11
} else {
    error("libX11 not found.")
}

exists( /usr/include/magic.h ) {
    LIBS += -lmagic
} else {
    error("libmagic not found.");
}

!packagesExist(glib-2.0 gobject-2.0) {
    error("Glib not found.");
}

!packagesExist(icu-uc icu-io icu-i18n) {
    error("icu not found.");
}

PKGCONFIG += glib-2.0 gobject-2.0 icu-uc icu-io icu-i18n

exists( /usr/include/nepomuk/filequery.h ) {
    lessThan(QT_MAJOR_VERSION, 5) {
        CONFIG += use_nepomuk
        DEFINES += WITH_NEPOMUK=1
        LIBS += -lkio -lkdecore -lsoprano -lnepomuk -lnepomukquery -lnepomukutils
        message("Nepomuk support: YES")
    }
}

!use_nepomuk {
    message("Nepomuk support: NO")
}

system( recoll -h > /dev/null ) {
    CONFIG += use_recoll
    DEFINES += WITH_RECOLL=1
    message("Recoll support: YES")
} else {
    message("Recoll support: NO")
}

VERSION = 3.5.1

SVNREV = $$system(svnversion .)
PLATFORM = $$system(uname -s)
BDATE = $$system(date +%F)

DEFINES += BUILD_REV=\\\"$$SVNREV\\\"
DEFINES += BUILD_PLATFORM=\\\"$$PLATFORM\\\"
DEFINES += BUILD_DATE=\\\"$$BDATE\\\"
DEFINES += BUILD_VERSION=\\\"$$VERSION\\\"

include( qtsingleapplication/src/qtsingleapplication.pri )
include( arabica/arabica.pri )
include( miniqxt/miniqxt.pri )

# install
sources.files = $$SOURCES \
    $$HEADERS \
    $$FORMS \
    jpreader.pro
target.path = /usr/local/bin
INSTALLS += target

OTHER_FILES += \
    img/startpage.html \
    org.jpreader.auxtranslator.xml \
    org.qjrad.dictionary.xml

DBUS_ADAPTORS = org.jpreader.auxtranslator.xml
DBUS_INTERFACES = org.qjrad.dictionary.xml
