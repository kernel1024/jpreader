INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

OTHER_FILES += $$PWD/css/default.css

SOURCES += \
    $$PWD/html/CharsetConverter.cc \
    $$PWD/html/Extensions.cc \
    $$PWD/html/Node.cc \
    $$PWD/html/ParserDom.cc \
    $$PWD/html/ParserSax.cc \
    $$PWD/html/Uri.cc \
    $$PWD/html/utils.cc \
    $$PWD/css/parser_pp.cc \
    $$PWD/css/css_lex.c \
    $$PWD/css/css_syntax.c \
    $$PWD/css/parser.c

HEADERS += \
    $$PWD/html/CharsetConverter.h \
    $$PWD/html/ci_string.h \
    $$PWD/html/debug.h \
    $$PWD/html/Extensions.h \
    $$PWD/html/Node.h \
    $$PWD/html/ParserDom.h \
    $$PWD/html/ParserSax.h \
    $$PWD/html/tree.h \
    $$PWD/html/tld.h \
    $$PWD/html/Uri.h \
    $$PWD/html/utils.h \
    $$PWD/html/wincstring.h \
    $$PWD/css/css_lex.h \
    $$PWD/css/css_syntax.h \
    $$PWD/css/parser.h \
    $$PWD/css/parser_pp.h
