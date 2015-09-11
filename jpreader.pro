HEADERS = mainwindow.h \
    snviewer.h \
    settingsdlg.h \
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
    indexersearch.h \
    translator.h \
    snwaitctl.h \
    searchmodel.h \
    titlestranslator.h \
    logdisplay.h \
    baloo5search.h \
    abstractthreadedsearch.h \
    abstracttranslator.h \
    bingtranslator.h \
    goldendictmgr.h \
    auxdictionary.h \
    multiinputdialog.h

SOURCES = main.cpp \
    mainwindow.cpp \
    settingsdlg.cpp \
    snviewer.cpp \
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
    indexersearch.cpp \
    translator.cpp \
    snwaitctl.cpp \
    titlestranslator.cpp \
    logdisplay.cpp \
    baloo5search.cpp \
    abstractthreadedsearch.cpp \
    abstracttranslator.cpp \
    bingtranslator.cpp \
    searchmodel.cpp \
    goldendictmgr.cpp \
    auxdictionary.cpp \
    multiinputdialog.cpp

RESOURCES = \
    jpreader.qrc

FORMS = main.ui \
    settingsdlg.ui \
    authdlg.ui \
    bookmarkdlg.ui \
    snviewer.ui \
    searchtab.ui \
    lighttranslator.ui \
    logdisplay.ui \
    auxdictionary.ui \
    multiinputdialog.ui

QT += network xml dbus widgets webenginewidgets

CONFIG += warn_on \
    link_pkgconfig \
    exceptions \
    rtti \
    stl \
    c++11

LIBS += -lz

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

exists( $$[QT_INSTALL_HEADERS]/QtWebEngineCore/QWebEngineUrlRequestInterceptor ) {
    DEFINES += WEBENGINE_56
    message("QtWebEngine is 5.6.0 or newer")
}

!packagesExist(icu-uc icu-io icu-i18n) {
    error("icu not found.");
}

PKGCONFIG += icu-uc icu-io icu-i18n

exists( /usr/include/KF5/Baloo/Baloo/Query ) {
    INCLUDEPATH += /usr/include/KF5
    INCLUDEPATH += /usr/include/KF5/Baloo
    CONFIG += use_baloo5
    DEFINES += WITH_BALOO5=1
    DEFINES += WITH_THREADED_SEARCH=1
    LIBS += -lKF5Baloo
    message("KF5 Baloo support: YES")
}

!use_baloo5 {
    message("KF5 Baloo support: NO")
}

system( which recoll > /dev/null 2>&1 ) {
    CONFIG += use_recoll
    DEFINES += WITH_RECOLL=1
    !use_baloo5 {
        DEFINES += WITH_THREADED_SEARCH=1
    }
    message("Recoll support: YES")
} else {
    message("Recoll support: NO")
}

GITREV = $$system(git rev-list HEAD|head -n1|cut -c -7)
PLATFORM = $$system(uname -s)
BDATE = $$system(date +%F)

DEFINES += BUILD_REV=\\\"$$GITREV\\\"
DEFINES += BUILD_PLATFORM=\\\"$$PLATFORM\\\"
DEFINES += BUILD_DATE=\\\"$$BDATE\\\"

include( arabica/arabica.pri )
include( miniqxt/miniqxt.pri )
include( goldendict/goldendict.pri )

# install
sources.files = $$SOURCES \
    $$HEADERS \
    $$FORMS \
    jpreader.pro
target.path = /usr/local/bin
INSTALLS += target

OTHER_FILES += \
    data/startpage.html \
    data/article-style.css \
    org.jpreader.auxtranslator.xml

DBUS_ADAPTORS = org.jpreader.auxtranslator.xml

DISTFILES += \
    README.md
