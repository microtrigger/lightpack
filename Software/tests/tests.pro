#-------------------------------------------------
#
# Project created by QtCreator 2011-09-09T12:09:24
#
#-------------------------------------------------

QT         += network testlib

QT         += widgets

TARGET      = LightpackTests
DESTDIR     = bin

CONFIG     += console
CONFIG     -= app_bundle

TEMPLATE    = app

# QMake and GCC produce a lot of stuff
OBJECTS_DIR = stuff
MOC_DIR     = stuff
UI_DIR      = stuff
RCC_DIR     = stuff


DEFINES += SRCDIR=\\\"$$PWD/\\\"

INCLUDEPATH += ../src/ ../src/grab ../hooks
SOURCES += \
    ../src/Settings.cpp \
#    ../src/LightpackPluginInterface.cpp \
#    ../src/plugins/PyPlugin.cpp \
    ../src/grab/calculations.cpp \
    SettingsWindowMockup.cpp \
    main.cpp \
    GrabCalculationTest.cpp \
    lightpackmathtest.cpp \
    ../src/LightpackMath.cpp

HEADERS += \
    ../src/grab/calculations.hpp \
    ../common/defs.h \
    ../src/enums.hpp \
#    ../src/ApiServerSetColorTask.hpp \
#    ../src/ApiServer.hpp \
    ../src/debug.h \
    ../src/Settings.hpp \
#    ../src/LightpackPluginInterface.hpp \
    SettingsWindowMockup.hpp \
    GrabCalculationTest.hpp \
    lightpackmathtest.hpp \
    ../src/LightpackMath.hpp


#
# PythonQt
#
#include (../PythonQt/build/common.prf )
#include (../PythonQt/build/PythonQt.prf )
#include (../PythonQt/build/PythonQt_QtAll.prf )
