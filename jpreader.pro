HEADERS = mainwindow.h \
    abstractthreadworker.h \
    browser-utils/autofillassistant.h \
    browser-utils/downloadlistmodel.h \
    browser-utils/downloadmodel.h \
    browser/browser.h \
    browser/ctxhandler.h \
    browser/msghandler.h \
    browser/net.h \
    browser/trans.h \
    browser/waitctl.h \
    global/actions.h \
    global/browserfuncs.h \
    global/control.h \
    global/control_p.h \
    global/history.h \
    global/network.h \
    global/pythonfuncs.h \
    global/pythonfuncs_p.h \
    global/ui.h \
    manga/mangaviewtab.h \
    manga/scalefilter.h \
    manga/zmangaview.h \
    manga/zscrollarea.h \
    search/baloosearch.h \
    search/defaultsearch.h \
    search/xapianindexworker.h \
    search/xapianindexworker_p.h \
    search/xapiansearch.h \
    translator-workers/abstracttranslator.h \
    translator-workers/alicloudtranslator.h \
    translator-workers/awstranslator.h \
    translator-workers/atlastranslator.h \
    translator-workers/deeplapitranslator.h \
    translator-workers/deeplfreetranslator.h \
    translator-workers/openaitranslator.h \
    translator-workers/promtnmttranslator.h \
    translator-workers/promtonefreetranslator.h \
    translator-workers/yandexcloudtranslator.h \
    translator-workers/yandextranslator.h \
    translator-workers/googlecloudtranslator.h \
    translator-workers/bingtranslator.h \
    translator-workers/googlegtxtranslator.h \
    translator-workers/webapiabstracttranslator.h \
    extractors/abstractextractor.h \
    extractors/htmlimagesextractor.h \
    extractors/fanboxextractor.h \
    extractors/patreonextractor.h \
    extractors/pixivindexextractor.h \
    extractors/pixivnovelextractor.h \
    extractors/deviantartextractor.h \
    global/structures.h \
    global/settings.h \
    global/contentfiltering.h \
    global/startup.h \
    browser-utils/authdlg.h \
    browser-utils/adblockrule.h \
    browser-utils/downloadmanager.h \
    browser-utils/downloadwriter.h \
    browser-utils/userscript.h \
    browser-utils/browsercontroller.h \
    browser-utils/bookmarks.h \
    browser-utils/xbel.h \
    browser-utils/noscriptdialog.h \
    search/searchtab.h \
    search/recollsearch.h \
    search/indexersearch.h \
    search/searchmodel.h \
    search/abstractthreadedsearch.h \
    translator/lighttranslator.h \
    translator/auxtranslator.h \
    translator/translator.h \
    translator/titlestranslator.h \
    translator/translatorcache.h \
    translator/translatorcachedialog.h \
    translator/translatorstatisticstab.h \
    translator/uitranslator.h \
    utils/htmlparser.h \
    utils/pdfworkerprivate.h \
    utils/pixivindexlimitsdialog.h \
    utils/sequencescheduler.h \
    utils/settingstab.h \
    utils/genericfuncs.h \
    utils/cliworker.h \
    utils/pixivindextab.h \
    utils/logdisplay.h \
    utils/auxdictionary.h \
    utils/multiinputdialog.h \
    utils/pdftotext.h \
    utils/sourceviewer.h \
    utils/edittreeview.h \
    utils/workermonitor.h \
    utils/specwidgets.h \
    jsedit/jsedit.h \
    jsedit/jsedit_p.h

SOURCES = main.cpp \
    browser-utils/autofillassistant.cpp \
    browser-utils/downloadlistmodel.cpp \
    browser-utils/downloadmodel.cpp \
    browser/browser.cpp \
    browser/ctxhandler.cpp \
    browser/msghandler.cpp \
    browser/net.cpp \
    browser/trans.cpp \
    browser/waitctl.cpp \
    global/actions.cpp \
    global/browserfuncs.cpp \
    global/control.cpp \
    global/control_p.cpp \
    global/history.cpp \
    global/network.cpp \
    global/pythonfuncs.cpp \
    global/ui.cpp \
    mainwindow.cpp \
    abstractthreadworker.cpp \
    manga/mangaviewtab.cpp \
    manga/scalefilter.cpp \
    manga/zmangaview.cpp \
    manga/zscrollarea.cpp \
    search/baloosearch.cpp \
    search/defaultsearch.cpp \
    search/xapianindexworker.cpp \
    search/xapiansearch.cpp \
    translator-workers/alicloudtranslator.cpp \
    translator-workers/awstranslator.cpp \
    translator-workers/atlastranslator.cpp \
    translator-workers/abstracttranslator.cpp \
    translator-workers/bingtranslator.cpp \
    translator-workers/deeplapitranslator.cpp \
    translator-workers/deeplfreetranslator.cpp \
    translator-workers/openaitranslator.cpp \
    translator-workers/promtnmttranslator.cpp \
    translator-workers/promtonefreetranslator.cpp \
    translator-workers/yandexcloudtranslator.cpp \
    translator-workers/yandextranslator.cpp \
    translator-workers/googlecloudtranslator.cpp \
    translator-workers/googlegtxtranslator.cpp \
    translator-workers/webapiabstracttranslator.cpp \
    extractors/abstractextractor.cpp \
    extractors/fanboxextractor.cpp \
    extractors/htmlimagesextractor.cpp \
    extractors/patreonextractor.cpp \
    extractors/pixivindexextractor.cpp \
    extractors/pixivnovelextractor.cpp \
    extractors/deviantartextractor.cpp \
    global/structures.cpp \
    global/settings.cpp \
    global/contentfiltering.cpp \
    global/startup.cpp \
    browser-utils/authdlg.cpp \
    browser-utils/adblockrule.cpp \
    browser-utils/downloadmanager.cpp \
    browser-utils/downloadwriter.cpp \
    browser-utils/userscript.cpp \
    browser-utils/browsercontroller.cpp \
    browser-utils/bookmarks.cpp \
    browser-utils/xbel.cpp \
    browser-utils/noscriptdialog.cpp \
    search/searchtab.cpp \
    search/recollsearch.cpp \
    search/indexersearch.cpp \
    search/abstractthreadedsearch.cpp \
    search/searchmodel.cpp \
    translator/lighttranslator.cpp \
    translator/auxtranslator.cpp \
    translator/translator.cpp \
    translator/titlestranslator.cpp \
    translator/translatorcache.cpp \
    translator/translatorcachedialog.cpp \
    translator/translatorstatisticstab.cpp \
    translator/uitranslator.cpp \
    utils/cliworker.cpp \
    utils/htmlparser.cpp \
    utils/pixivindexlimitsdialog.cpp \
    utils/pixivindextab.cpp \
    utils/pdfworkerprivate.cpp \
    utils/sequencescheduler.cpp \
    utils/settingstab.cpp \
    utils/genericfuncs.cpp \
    utils/logdisplay.cpp \
    utils/auxdictionary.cpp \
    utils/multiinputdialog.cpp \
    utils/pdftotext.cpp \
    utils/sourceviewer.cpp \
    utils/edittreeview.cpp \
    utils/workermonitor.cpp \
    utils/specwidgets.cpp \
    jsedit/jsedit.cpp

RESOURCES = \
    jpreader.qrc

FORMS = main.ui \
    browser-utils/autofillassistant.ui \
    browser/browser.ui \
    manga/mangaviewtab.ui \
    search/searchtab.ui \
    translator/lighttranslator.ui \
    translator/translatorcachedialog.ui \
    translator/translatorstatisticstab.ui \
    browser-utils/authdlg.ui \
    browser-utils/downloadlistdlg.ui \
    browser-utils/downloadmanager.ui \
    browser-utils/userscriptdlg.ui \
    browser-utils/bookmarks.ui \
    browser-utils/addbookmarkdialog.ui \
    browser-utils/noscriptdialog.ui \
    utils/pixivindexlimitsdialog.ui \
    utils/pixivindextab.ui \
    utils/settingstab.ui \
    utils/logdisplay.ui \
    utils/auxdictionary.ui \
    utils/multiinputdialog.ui \
    utils/sourceviewer.ui \
    utils/hashviewer.ui \
    utils/workermonitor.ui

QT += network xml dbus widgets webenginewidgets printsupport testlib

CONFIG += warn_on \
    link_pkgconfig \
    exceptions \
    rtti \
    stl \
    c++17

!versionAtLeast(QT_VERSION, 6.2.0):error("Use at least Qt version 6.2.0")

# warn on *any* usage of deprecated APIs
DEFINES += QT_DEPRECATED_WARNINGS
# ... and just fail to compile if APIs deprecated in Qt <= 6.2 are used
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060200

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_BYTEARRAY

LIBS += -lz -ltbb
# for deprecations in GCC 11-12 without oneTBB
DEFINES += TBB_SUPPRESS_DEPRECATED_MESSAGES

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

PKGCONFIG += icu-uc icu-io icu-i18n openssl

exists( /usr/include/KF6/Baloo/Baloo/Query ) {
    INCLUDEPATH += /usr/include/KF6
    INCLUDEPATH += /usr/include/KF6/Baloo
    CONFIG += use_baloo
    DEFINES += WITH_BALOO=1
    LIBS += -lKF6Baloo
    message("KF6 Baloo support: YES")
}

!use_baloo {
    message("KF6 Baloo support: NO")
}

system( which recoll > /dev/null 2>&1 ) {
    CONFIG += use_recoll
    DEFINES += WITH_RECOLL=1
    message("Recoll support: YES")
} else {
    message("Recoll support: NO")
}

packagesExist(poppler) {
    CONFIG += use_poppler
}

use_poppler {
    DEFINES += WITH_POPPLER=1
    PKGCONFIG += poppler
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

packagesExist(xapian-core-1.5) {
    CONFIG += use_xapian
}

use_xapian {
    DEFINES += WITH_XAPIAN=1
    PKGCONFIG += xapian-core-1.5
    message("Using Xapian: YES")
} else {
    message("Using Xapian: NO")
}

packagesExist(python3-embed) {
    CONFIG += use_python3_embed
}

use_python3_embed {
    DEFINES += WITH_PYTHON3=1
    PKGCONFIG += python3-embed
    message("Using Python3: YES")
} else {
    message("Using Python3: NO")
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
