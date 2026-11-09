QT += core gui concurrent quick testlib
linux: QT += dbus
CONFIG += testcase c++17
include(../syntax-highlighting.pri)
include(../spellchecking.pri)

TEMPLATE = app
TARGET = tst_ponchowriter

INCLUDEPATH += ../src
SOURCES += \
    tst_ponchowriter.cpp \
    ../src/backend.cpp \
    ../src/windowmanager.cpp \
    ../src/fileopenrequests.cpp \
    ../src/systemtheme.cpp \
    ../src/spellingcontroller.cpp \
    ../src/spellingprose.cpp \
    ../src/cli.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/livepreview.cpp \
    ../src/previewblocks.cpp \
    ../src/mermaidrenderer.cpp
HEADERS += \
    ../src/backend.h \
    ../src/windowmanager.h \
    ../src/fileopenrequests.h \
    ../src/systemtheme.h \
    ../src/spellingcontroller.h \
    ../src/spellingprose.h \
    ../src/cli.h \
    ../src/markdownhighlighter.h \
    ../src/livepreview.h \
    ../src/previewblocks.h \
    ../src/mermaidrenderer.h

QT += widgets printsupport quickcontrols2 quickdialogs2
RESOURCES += ../src/resources.qrc
