HEADERS = mainwindow.h \
    cliworker.h \
    extractors/deviantartextractor.h \
    pixivindextab.h \
    translatorcachedialog.h \
    translators/abstracttranslator.h \
    translators/alicloudtranslator.h \
    translators/awstranslator.h \
    translators/atlastranslator.h \
    translators/yandexcloudtranslator.h \
    translators/yandextranslator.h \
    translators/googlecloudtranslator.h \
    translators/bingtranslator.h \
    translators/googlegtxtranslator.h \
    translators/webapiabstracttranslator.h \
    extractors/abstractextractor.h \
    extractors/htmlimagesextractor.h \
    extractors/fanboxextractor.h \
    extractors/patreonextractor.h \
    extractors/pixivindexextractor.h \
    extractors/pixivnovelextractor.h \
    downloadwriter.h \
    globalprivate.h \
    jsedit/jsedit_p.h \
    pdfworkerprivate.h \
    settingstab.h \
    snviewer.h \
    specwidgets.h \
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
    auxdictionary.h \
    multiinputdialog.h \
    adblockrule.h \
    downloadmanager.h \
    translatorcache.h \
    translatorstatisticstab.h \
    pdftotext.h \
    sourceviewer.h \
    structures.h \
    settings.h \
    globalui.h \
    uitranslator.h \
    userscript.h \
    browsercontroller.h \
    bookmarks.h \
    edittreeview.h \
    xbel.h \
    noscriptdialog.h \
    jsedit/jsedit.h

SOURCES = main.cpp \
    cliworker.cpp \
    extractors/deviantartextractor.cpp \
    pixivindextab.cpp \
    translatorcachedialog.cpp \
    translators/alicloudtranslator.cpp \
    translators/awstranslator.cpp \
    translators/atlastranslator.cpp \
    translators/abstracttranslator.cpp \
    translators/bingtranslator.cpp \
    translators/yandexcloudtranslator.cpp \
    translators/yandextranslator.cpp \
    translators/googlecloudtranslator.cpp \
    translators/googlegtxtranslator.cpp \
    translators/webapiabstracttranslator.cpp \
    extractors/abstractextractor.cpp \
    extractors/fanboxextractor.cpp \
    extractors/htmlimagesextractor.cpp \
    extractors/patreonextractor.cpp \
    extractors/pixivindexextractor.cpp \
    extractors/pixivnovelextractor.cpp \
    downloadwriter.cpp \
    globalprivate.cpp \
    mainwindow.cpp \
    pdfworkerprivate.cpp \
    settingstab.cpp \
    snviewer.cpp \
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
    searchmodel.cpp \
    auxdictionary.cpp \
    multiinputdialog.cpp \
    adblockrule.cpp \
    downloadmanager.cpp \
    translatorcache.cpp \
    translatorstatisticstab.cpp \
    pdftotext.cpp \
    sourceviewer.cpp \
    structures.cpp \
    settings.cpp \
    globalui.cpp \
    uitranslator.cpp \
    userscript.cpp \
    browsercontroller.cpp \
    bookmarks.cpp \
    edittreeview.cpp \
    xbel.cpp \
    noscriptdialog.cpp \
    jsedit/jsedit.cpp

RESOURCES = \
    jpreader.qrc

FORMS = main.ui \
    authdlg.ui \
    downloadlistdlg.ui \
    pixivindextab.ui \
    settingstab.ui \
    snviewer.ui \
    searchtab.ui \
    lighttranslator.ui \
    logdisplay.ui \
    auxdictionary.ui \
    multiinputdialog.ui \
    downloadmanager.ui \
    sourceviewer.ui \
    translatorcachedialog.ui \
    translatorstatisticstab.ui \
    userscriptdlg.ui \
    bookmarks.ui \
    addbookmarkdialog.ui \
    noscriptdialog.ui \
    hashviewer.ui

QT += network xml dbus widgets webenginewidgets x11extras printsupport testlib

CONFIG += warn_on \
    link_pkgconfig \
    exceptions \
    rtti \
    stl \
    c++17

# warn on *any* usage of deprecated APIs
DEFINES += QT_DEPRECATED_WARNINGS
# ... and just fail to compile if APIs deprecated in Qt <= 5.15 are used
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00

LIBS += -lz

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

PKGCONFIG += icu-uc icu-io icu-i18n xcb xcb-keysyms openssl

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
include( zdict/zdict.pri )

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
