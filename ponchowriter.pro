QT += core gui concurrent widgets printsupport qml quick quickcontrols2 quickdialogs2
linux: QT += dbus

!versionAtLeast(QT_VERSION, 6.7.0): error("Native live preview requires Qt 6.7 or newer")

CONFIG += c++17 release
TARGET = ponchowriter
include(syntax-highlighting.pri)
include(spellchecking.pri)

TEMPLATE = app
macx {
    ICON = assets/ponchowriter.icns
    QMAKE_INFO_PLIST = assets/Info.plist
    syntaxNotices.files = $$PWD/third_party/ksyntaxhighlighting
    syntaxNotices.path = Contents/Resources/third_party
    spellNotices.files = $$PWD/third_party/sonnet
    spellNotices.path = Contents/Resources/third_party
    appNotices.files = $$PWD/LICENSE $$PWD/COPYRIGHT.md
    appNotices.path = Contents/Resources
    fontNotice.files = $$PWD/fonts/OFL.txt
    fontNotice.path = Contents/Resources/fonts
    QMAKE_BUNDLE_DATA += syntaxNotices spellNotices appNotices fontNotice
}

HEADERS += \
    src/profile.h \
    src/backend.h \
    src/printdocument.h \
    src/windowmanager.h \
    src/instancebroker.h \
    src/fileopenrequests.h \
    src/spellingcontroller.h \
    src/spellingprose.h \
    src/cli.h \
    src/markdowntablemodel.h \
    src/livepreview.h \
    src/listpreview.h \
    src/mermaidrenderer.h \
    src/previewblocks.h \
    src/markdownhighlighter.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/windowmanager.cpp \
    src/instancebroker.cpp \
    src/fileopenrequests.cpp \
    src/backend.cpp \
    src/printdocument.cpp \
    src/spellingcontroller.cpp \
    src/spellingprose.cpp \
    src/cli.cpp \
    src/markdowntablemodel.cpp \
    src/livepreviewtables.cpp \
    src/livepreview.cpp \
    src/mermaidrenderer.cpp \
    src/previewblocks.cpp \
    src/markdownhighlighter.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc
