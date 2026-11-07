cmake_minimum_required(VERSION 3.24)
project(OmaWriteSyntaxDependency LANGUAGES CXX)
include(ExternalProject)
find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Network)

# Standalone, pinned Kate syntax engine. No Kate editor, WebEngine, or desktop.
set(prefix "${CMAKE_BINARY_DIR}/install")
ExternalProject_Add(ecm
    URL https://download.kde.org/stable/frameworks/6.13/extra-cmake-modules-6.13.0.tar.xz
    URL_HASH SHA256=7006017c00c817ff4c056995146d271791d1487a398d39ea6cac1cd59a8bf402
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${prefix}
        -DBUILD_TESTING:BOOL=OFF -DBUILD_HTML_DOCS:BOOL=OFF
        -DBUILD_QTHELP_DOCS:BOOL=OFF -DBUILD_MAN_DOCS:BOOL=OFF
)
ExternalProject_Add(syntax-highlighting
    DEPENDS ecm
    URL https://download.kde.org/stable/frameworks/6.13/syntax-highlighting-6.13.0.tar.xz
    URL_HASH SHA256=d6f19bb886d6e1c7251d813497f14ecc9dac413253764eb61a8f3a836806450d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${prefix}
        -DECM_DIR:PATH=${prefix}/share/ECM/cmake -DQt6_DIR:PATH=${Qt6_DIR}
        -DCMAKE_BUILD_TYPE:STRING=Release -DBUILD_SHARED_LIBS:BOOL=OFF
        -DBUILD_TESTING:BOOL=OFF -DBUILD_QCH:BOOL=OFF -DKF_SKIP_PO_PROCESSING:BOOL=ON
        -DKDE_INSTALL_LIBDIR:PATH=lib -DKDE_INSTALL_INCLUDEDIR_KF:PATH=include/KF6
        -DKSYNTAXHIGHLIGHTING_USE_GUI:BOOL=ON -DQRC_SYNTAX:BOOL=ON
        -DBUILD_codeeditor:BOOL=OFF -DBUILD_codepdfprinter:BOOL=OFF -DBUILD_minimaltest:BOOL=OFF
        -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Quick:BOOL=ON -DCMAKE_DISABLE_FIND_PACKAGE_XercesC:BOOL=ON
        -DCMAKE_CXX_STANDARD:STRING=17 -DCMAKE_CXX_STANDARD_REQUIRED:BOOL=ON
        -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release
)
