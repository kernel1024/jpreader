HEADERS = mainwindow.h \
    awstranslator.h \
    globalprivate.h \
    pdfworkerprivate.h \
    pixivindexextractor.h \
    settingstab.h \
    snviewer.h \
    specwidgets.h \
    atlastranslator.h \
    genericfuncs.h \
    globalcontrol.h \
    snmsghandler.h \
    snctxhandler.h \
    sntrans.h \
    snnet.h \
    authdlg.h \
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
    auxdictionary.h \
    multiinputdialog.h \
    adblockrule.h \
    downloadmanager.h \
    translatorcache.h \
    translatorstatisticstab.h \
    yandexcloudtranslator.h \
    yandextranslator.h \
    pdftotext.h \
    sourceviewer.h \
    structures.h \
    settings.h \
    globalui.h \
    userscript.h \
    browsercontroller.h \
    googlegtxtranslator.h \
    webapiabstracttranslator.h \
    bookmarks.h \
    edittreeview.h \
    xbel.h \
    pixivnovelextractor.h \
    noscriptdialog.h \
    jsedit/jsedit.h

SOURCES = main.cpp \
    awstranslator.cpp \
    globalprivate.cpp \
    mainwindow.cpp \
    pdfworkerprivate.cpp \
    pixivindexextractor.cpp \
    settingstab.cpp \
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
    auxdictionary.cpp \
    multiinputdialog.cpp \
    adblockrule.cpp \
    downloadmanager.cpp \
    translatorcache.cpp \
    translatorstatisticstab.cpp \
    yandexcloudtranslator.cpp \
    yandextranslator.cpp \
    pdftotext.cpp \
    sourceviewer.cpp \
    structures.cpp \
    settings.cpp \
    globalui.cpp \
    userscript.cpp \
    browsercontroller.cpp \
    googlegtxtranslator.cpp \
    webapiabstracttranslator.cpp \
    bookmarks.cpp \
    edittreeview.cpp \
    xbel.cpp \
    pixivnovelextractor.cpp \
    noscriptdialog.cpp \
    jsedit/jsedit.cpp

RESOURCES = \
    jpreader.qrc

FORMS = main.ui \
    authdlg.ui \
    settingstab.ui \
    snviewer.ui \
    searchtab.ui \
    lighttranslator.ui \
    logdisplay.ui \
    auxdictionary.ui \
    multiinputdialog.ui \
    downloadmanager.ui \
    sourceviewer.ui \
    translatorstatisticstab.ui \
    userscriptdlg.ui \
    selectablelistdlg.ui \
    bookmarks.ui \
    addbookmarkdialog.ui \
    noscriptdialog.ui \
    hashviewer.ui

QT += network xml dbus widgets webenginewidgets x11extras printsupport testlib concurrent

CONFIG += warn_on \
    link_pkgconfig \
    exceptions \
    rtti \
    stl \
    c++14

# warn on *any* usage of deprecated APIs
DEFINES += QT_DEPRECATED_WARNINGS
# ... and just fail to compile if APIs deprecated in Qt <= 5.11 are used
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050B00

LIBS += -lz -lgoldendict

exists( /usr/include/magic.h ) {
    LIBS += -lmagic
} else {
    error("libmagic not found.");
}

!packagesExist(icu-uc icu-io icu-i18n) {
    error("icu not found.");
}

packagesExist(libzip) {
    PKGCONFIG += libzip
} else {
    error("libzip not found.")
}

PKGCONFIG += icu-uc icu-io icu-i18n xcb xcb-keysyms

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

packagesExist(poppler-cpp) {
    packagesExist(poppler) {
        CONFIG += use_poppler
    }
}

use_poppler {
    DEFINES += WITH_POPPLER=1
    PKGCONFIG += poppler-cpp poppler
    message("Using Poppler: YES")
} else {
    message("Using Poppler: NO")
}

packagesExist(source-highlight) {
    CONFIG += use_source-highlight
}

use_source-highlight {
    DEFINES += WITH_SRCHILITE=1
    PKGCONFIG += source-highlight
    message("Using source-highlight: YES");
} else {
    message("Using source-highlight: NO");
}

GITREV = $$system(git rev-list HEAD|head -n1|cut -c -7)
PLATFORM = $$system(uname -s)
BDATE = $$system(date +%F)

DEFINES += BUILD_REV=\\\"$$GITREV\\\"
DEFINES += BUILD_PLATFORM=\\\"$$PLATFORM\\\"
DEFINES += BUILD_DATE=\\\"$$BDATE\\\"

include( htmlcxx/htmlcxx.pri )
include( miniqxt/miniqxt.pri )
include( qcustomplot/qcustomplot.pri )

OTHER_FILES += \
    data/startpage.html \
    data/article-style.css \
    data/userscript.js

DBUS_ADAPTORS = org.kernel1024.jpreader.auxtranslator.xml \
    org.kernel1024.jpreader.browsercontroller.xml

DISTFILES += \
    .clang-tidy \
    README.md \
    org.kernel1024.jpreader.auxtranslator.xml \
    org.kernel1024.jpreader.browsercontroller.xml
