QT += core gui widgets concurrent printsupport qml quick testlib
CONFIG += c++17 testcase console
include(../../syntax-highlighting.pri)

TEMPLATE = app
TARGET = tst_attachments
INCLUDEPATH += ../../src
SOURCES += tst_attachments.cpp ../../src/backend.cpp ../../src/markdownhighlighter.cpp
HEADERS += ../../src/backend.h ../../src/attachmenthelper.h ../../src/markdownhighlighter.h
