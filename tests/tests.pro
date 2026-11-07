QT += core gui concurrent quick testlib
CONFIG += testcase c++17
include(../syntax-highlighting.pri)
include(../spellchecking.pri)

TEMPLATE = app
TARGET = tst_omawrite

INCLUDEPATH += ../src
SOURCES += \
    tst_omawrite.cpp \
    ../src/backend.cpp \
    ../src/spellingcontroller.cpp \
    ../src/spellingprose.cpp \
    ../src/cli.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/livepreview.cpp \
    ../src/previewblocks.cpp \
    ../src/mermaidrenderer.cpp
HEADERS += \
    ../src/backend.h \
    ../src/spellingcontroller.h \
    ../src/spellingprose.h \
    ../src/cli.h \
    ../src/markdownhighlighter.h \
    ../src/livepreview.h \
    ../src/previewblocks.h \
    ../src/mermaidrenderer.h

QT += widgets printsupport quickcontrols2 quickdialogs2
