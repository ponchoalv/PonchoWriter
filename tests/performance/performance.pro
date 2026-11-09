QT += core gui concurrent quick testlib widgets printsupport quickcontrols2 quickdialogs2
linux: QT += dbus
CONFIG += testcase c++17 release
CONFIG -= debug
include(../../syntax-highlighting.pri)
include(../../spellchecking.pri)
TEMPLATE = app
TARGET = tst_performance
INCLUDEPATH += ../../src
SOURCES += tst_performance.cpp \
    ../../src/backend.cpp ../../src/systemtheme.cpp \
    ../../src/spellingcontroller.cpp ../../src/spellingprose.cpp \
    ../../src/markdownhighlighter.cpp ../../src/livepreview.cpp \
    ../../src/previewblocks.cpp ../../src/mermaidrenderer.cpp
HEADERS += ../../src/backend.h ../../src/systemtheme.h \
    ../../src/spellingcontroller.h ../../src/spellingprose.h \
    ../../src/markdownhighlighter.h ../../src/livepreview.h \
    ../../src/previewblocks.h ../../src/mermaidrenderer.h
RESOURCES += ../../src/resources.qrc
