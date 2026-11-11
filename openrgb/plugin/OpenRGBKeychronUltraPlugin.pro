#-----------------------------------------------------------------------------#
# OpenRGB Keychron V6 Ultra 8K plugin (custom ZMK firmware, issue #893)        #
# Builds a Qt plugin DLL for OpenRGB 1.0rc3 (plugin API v4).                   #
#-----------------------------------------------------------------------------#

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = lib
CONFIG  += plugin c++17
TARGET   = OpenRGBKeychronUltraPlugin

#-----------------------------------------------------------------------------#
# Plugin sources                                                              #
#-----------------------------------------------------------------------------#
HEADERS += \
    OpenRGBKeychronUltraPlugin.h \
    KeychronUltraController.h \
    RGBController_KeychronUltra.h \
    KeychronLayouts.h

SOURCES += \
    OpenRGBKeychronUltraPlugin.cpp \
    KeychronUltraController.cpp \
    RGBController_KeychronUltra.cpp \
    KeychronLayouts.cpp

#-----------------------------------------------------------------------------#
# OpenRGB SDK (checked out at ./OpenRGB @ release_candidate_1.0rc3 by CI)      #
# We subclass RGBController, so its implementation is compiled in.            #
#-----------------------------------------------------------------------------#
INCLUDEPATH += \
    OpenRGB/ \
    OpenRGB/dependencies/json \
    OpenRGB/i2c_smbus \
    OpenRGB/net_port \
    OpenRGB/RGBController \
    OpenRGB/dependencies/hidapi-win/include

HEADERS += \
    OpenRGB/OpenRGBPluginInterface.h \
    OpenRGB/ResourceManagerInterface.h \
    OpenRGB/RGBController/RGBController.h

SOURCES += \
    OpenRGB/RGBController/RGBController.cpp

#-----------------------------------------------------------------------------#
# hidapi (Windows: link OpenRGB's bundled import lib; hidapi.dll ships with    #
# OpenRGB at runtime).                                                        #
#-----------------------------------------------------------------------------#
equals(QT_ARCH, i386): win32:LIBS += -L"$$PWD/OpenRGB/dependencies/hidapi-win/x86" -lhidapi
else:                   win32:LIBS += -L"$$PWD/OpenRGB/dependencies/hidapi-win/x64" -lhidapi

#-----------------------------------------------------------------------------#
# hidapi (Linux: link the system hidraw backend via pkg-config, from           #
# libhidapi-dev). The hidapi.h API header is the same cross-platform one       #
# already on INCLUDEPATH above.                                               #
#-----------------------------------------------------------------------------#
unix:!macx {
    CONFIG    += link_pkgconfig
    PKGCONFIG += hidapi-hidraw
}
