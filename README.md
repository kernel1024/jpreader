# jpreader

JPReader is a assistance tool for reading and translating text from Internet or local text repository.

Integrated with KDE Frameworks 5 Baloo API support, Recoll support (through CLI client output parser) and Xapian backend for local text repository, external ATLAS engine interface with online translation engines support for text translation.

Library dependencies:

    Qt 5.15 (core, gui, network, xml, webengine, dbus, printing, testlib)
    icu ( http://www.icu-project.org/ )
    libmagic
    intel-tbb
    zlib
    openssl
    recoll engine (optional)
    baloo 5.6.0 (optional)
    poppler 0.85 (optional)
    xapian-core 1.5.0 (optional)

Integrated static parts from libraries:

    htmlcxx 0.84
    libqxt 0.6.2
    jsedit from X2 project by Ariya Hidayat
    QCustomPlot 2.0.1 by Emanuel Eichhammer
