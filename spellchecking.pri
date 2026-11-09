# Sonnet Core with one platform backend, private static Release build.
PW_SPELL_PREFIX = $$(PONCHOWRITER_SPELL_PREFIX)
isEmpty(PW_SPELL_PREFIX): PW_SPELL_PREFIX = $$PWD/.deps/install
!exists($$PW_SPELL_PREFIX/include/KF6/SonnetCore/Sonnet/Speller) {
    error("Sonnet Core missing. Run bin/build-dependencies first (see docs/install.md).")
}
CONFIG -= debug debug_and_release
CONFIG += release
INCLUDEPATH += $$PW_SPELL_PREFIX/include/KF6/SonnetCore $$PW_SPELL_PREFIX/include/KF6/Sonnet
DEFINES += SONNETCORE_STATIC_DEFINE
SOURCES += $$PWD/dependencies/spellplugins.cpp
LIBS += -L$$quote($$PW_SPELL_PREFIX/lib/plugins/kf6/sonnet)
macx {
    LIBS += -lsonnet_nsspellchecker -framework AppKit
} else:win32 {
    LIBS += -lsonnet_ispellchecker -lole32
} else:linux {
    LIBS += -lsonnet_hunspell
    CONFIG += link_pkgconfig
    PKGCONFIG += hunspell
}
LIBS += -L$$quote($$PW_SPELL_PREFIX/lib) -lKF6SonnetCore
