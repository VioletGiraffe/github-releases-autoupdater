TARGET = autoupdater
TEMPLATE = lib
CONFIG += staticlib

QT = core network
!updater_without_widgets:QT += widgets gui

CONFIG += strict_c++
exists(../global.pri){
	include(../global.pri)
} else {
	CONFIG += c++2b
	win*:QMAKE_CXXFLAGS_WARN_ON = /W4

	mac*{
		# Qt frameworks as system headers: moc output includes them outside DISABLE_COMPILER_WARNINGS
		QMAKE_CXXFLAGS += -iframework $$[QT_INSTALL_LIBS]

		# Qt 6.9 headers use ARM ACLE intrinsics without including arm_acle.h
		contains(QMAKE_HOST.arch, arm64)|contains(QMAKE_APPLE_DEVICE_ARCHS, arm64) {
			QMAKE_CXXFLAGS += -include arm_acle.h
		}
	}
}

mac* | linux* | freebsd{
	CONFIG(release, debug|release):CONFIG *= Release optimize_full
	CONFIG(debug, debug|release):CONFIG *= Debug
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
	QMAKE_CXXFLAGS += /MP /Zi
	QMAKE_CXXFLAGS += /std:c++latest /permissive- /Zc:__cplusplus
	QMAKE_CXXFLAGS_WARN_ON += /wd4251
	DEFINES += WIN32_LEAN_AND_MEAN NOMINMAX

	Debug:QMAKE_LFLAGS += /INCREMENTAL
	Release:QMAKE_LFLAGS += /OPT:REF /OPT:ICF
}

mac* | linux* | freebsd{
	CONFIG += strict_c c99
	QMAKE_CFLAGS_WARN_ON   += -pedantic-errors
	QMAKE_CXXFLAGS_WARN_ON += -pedantic-errors
	QMAKE_CXXFLAGS_WARN_ON *= -Wall
	*-g++*:QMAKE_CXXFLAGS_WARN_ON += -Wno-maybe-uninitialized # False positives on std::optional and std::expected

	Release:DEFINES += NDEBUG=1
	Debug:DEFINES += _DEBUG
}

HEADERS += \
	src/cautoupdatergithub.h \
	src/updateinstaller.hpp

SOURCES += \
	src/cautoupdatergithub.cpp

win*:SOURCES += src/updateinstaller_win.cpp
else:SOURCES += src/updateinstaller_unsupported.cpp

!updater_without_widgets{
	SOURCES += \
		src/updaterUI/cupdaterdialog.cpp

	HEADERS += \
		src/updaterUI/cupdaterdialog.h
}
