QT += core gui concurrent widgets printsupport qml quick quickcontrols2 quickdialogs2
linux: QT += dbus

!versionAtLeast(QT_VERSION, 6.7.0): error("Native live preview requires Qt 6.7 or newer")

CONFIG += c++17 release
TARGET = omawrite
TEMPLATE = app
macx: ICON = assets/omawrite.icns

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/livepreview.h \
    src/mermaidrenderer.h \
    src/previewblocks.h \
    src/markdownhighlighter.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/livepreview.cpp \
    src/mermaidrenderer.cpp \
    src/previewblocks.cpp \
    src/markdownhighlighter.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc
