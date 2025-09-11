TARGET = autoupdater
TEMPLATE = lib
CONFIG += staticlib

QT = core network
!updater_without_widgets:QT += widgets gui

CONFIG += strict_c++ c++2b

mac* | linux* | freebsd{
	CONFIG(release, debug|release):CONFIG *= Release optimize_full
	CONFIG(debug, debug|release):CONFIG *= Debug
}

contains(QT_ARCH, x86_64) {
	ARCHITECTURE = x64
} else {
	ARCHITECTURE = x86
}

Release:OUTPUT_DIR=release/$${ARCHITECTURE}
Debug:OUTPUT_DIR=debug/$${ARCHITECTURE}

DESTDIR     = ../bin/$${OUTPUT_DIR}
OBJECTS_DIR = ../build/$${OUTPUT_DIR}/$${TARGET}
MOC_DIR     = ../build/$${OUTPUT_DIR}/$${TARGET}
UI_DIR      = ../build/$${OUTPUT_DIR}/$${TARGET}
RCC_DIR     = ../build/$${OUTPUT_DIR}/$${TARGET}

# Required for qDebug() to log function name, file and line in release build
DEFINES += QT_MESSAGELOGCONTEXT

INCLUDEPATH += \
	$${PWD}/3rdparty

win*{
	QMAKE_CXXFLAGS += /MP /Zi /wd4251
	QMAKE_CXXFLAGS += /std:c++latest /permissive- /Zc:__cplusplus
	QMAKE_CXXFLAGS_WARN_ON = /W4
	DEFINES += WIN32_LEAN_AND_MEAN NOMINMAX

	!*msvc2013*:QMAKE_LFLAGS += /DEBUG:FASTLINK

	Debug:QMAKE_LFLAGS += /INCREMENTAL
	Release:QMAKE_LFLAGS += /OPT:REF /OPT:ICF
}

mac* | linux* | freebsd{
	QMAKE_CFLAGS   += -pedantic-errors -std=c99
	QMAKE_CXXFLAGS += -pedantic-errors
	QMAKE_CXXFLAGS_WARN_ON = -Wall

	Release:DEFINES += NDEBUG=1
	Debug:DEFINES += _DEBUG
}

HEADERS += \
	src/cautoupdatergithub.h \
	src/updateinstaller.hpp

SOURCES += \
	src/cautoupdatergithub.cpp

win*:SOURCES += src/updateinstaller_win.cpp
mac*:SOURCES += src/updateinstaller_mac.cpp
linux*:SOURCES += src/updateinstaller_linux.cpp
freebsd:SOURCES += src/updateinstaller_freebsd.cpp

!updater_without_widgets{
	SOURCES += \
		src/updaterUI/cupdaterdialog.cpp

	HEADERS += \
		src/updaterUI/cupdaterdialog.h

	FORMS += \
		src/updaterUI/cupdaterdialog.ui
}
