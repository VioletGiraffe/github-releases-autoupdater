TARGET = autoupdater
TEMPLATE = lib
CONFIG += staticlib

QT = core network
CONFIG += c++14

!updater_without_widgets{
	QT += widgets gui
}

mac* | linux*{
	CONFIG(release, debug|release):CONFIG += Release
	CONFIG(debug, debug|release):CONFIG += Debug
}

Release:OUTPUT_DIR=release
Debug:OUTPUT_DIR=debug

DESTDIR     = ../bin/$${OUTPUT_DIR}
OBJECTS_DIR = ../build/$${OUTPUT_DIR}/$${TARGET}
MOC_DIR     = ../build/$${OUTPUT_DIR}/$${TARGET}
UI_DIR      = ../build/$${OUTPUT_DIR}/$${TARGET}
RCC_DIR     = ../build/$${OUTPUT_DIR}/$${TARGET}

# Required for qDebug() to log function name, file and line in release build
DEFINES += QT_MESSAGELOGCONTEXT

win*{
	QMAKE_CXXFLAGS += /MP /wd4251
	QMAKE_CXXFLAGS_WARN_ON = /W4
	DEFINES += WIN32_LEAN_AND_MEAN NOMINMAX

	!*msvc2013*:QMAKE_LFLAGS += /DEBUG:FASTLINK

	Debug:QMAKE_LFLAGS += /INCREMENTAL
	Release:QMAKE_LFLAGS += /OPT:REF /OPT:ICF
}

mac* | linux* {
	QMAKE_CFLAGS   += -pedantic-errors -std=c99
	QMAKE_CXXFLAGS += -pedantic-errors
	QMAKE_CXXFLAGS_WARN_ON = -Wall -Wno-c++11-extensions -Wno-local-type-template-args -Wno-deprecated-register

	Release:DEFINES += NDEBUG=1
	Debug:DEFINES += _DEBUG
}

HEADERS += \
	src/cautoupdatergithub.h

SOURCES += \
	src/cautoupdatergithub.cpp

!updater_without_widgets{
	SOURCES += \
		src/updaterUI/cupdaterdialog.cpp

	HEADERS += \
		src/updaterUI/cupdaterdialog.h

	FORMS += \
		src/updaterUI/cupdaterdialog.ui
}
