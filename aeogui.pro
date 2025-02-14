#-----------------------------------------------------------------------------
# This file is part of AEO-Light
#
# Copyright (c) 2016-2025 University of South Carolina
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# AEO-Light is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#
# Funding for AEO-Light development was provided through a grant from the
# National Endowment for the Humanities
#-----------------------------------------------------------------------------

# PREREQUISITES:
#
# place libraries and include files in system locations like /usr/local/lib
# on unix and osx, or under C:\include and C:\lib under windows, or put
# them or link them to the AEO-Light source directory tree either directly
# or under subdirectories called "release" and "debug"
#
# Or add the appropriate paths to the platform-specific sections of this
# project file
#
# Additional libraries required:
# libav (the ffmpeg libraries)
# dpx
# dspfilters
# openexr

QT       += core gui multimedia xml

win32: QT += opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = AEO-Light
TEMPLATE = app

# The version of AEO-Light
APP_NAME = AEO-Light
VERSION = 2.4

# nb: when updating the version number, be sure to update LICENSE.txt too

# Make the version number visible in the source code
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += APP_VERSION_STR=\\\"$$VERSION\\\"


ICON = $$PWD/aeolight.icns

#------------------------------------------------------------------------------
# platform-specific include paths
win32 {
	INCLUDEPATH += /include
        INCLUDEPATH += $$PWD/include
        DEFINES += __STDC_CONSTANT_MACROS
} else:unix {
	INCLUDEPATH += /usr/local/include/ /opt/local/include/
}

INCLUDEPATH += $$PWD/
DEPENDPATH += $$PWD/

# If DSP Filters are not on the include path yet...
#INCLUDEPATH += ../DSPFilters/shared/DSPFilters/include

#-----------------------------------------------------------------------------
# platform-specific linking
macx {
	QMAKE_LIBDIR += /usr/local/lib /opt/local/lib
	QMAKE_RPATHDIR += @executable_path/../Frameworks/
	QMAKE_LINK += -headerpad_max_install_names
} else:win32 {
	QMAKE_LIBDIR += "C:\lib"
        QMAKE_LIBDIR += $$PWD/lib
	LIBS += -lopengl32
} else:unix {
	LIBS += -lGL
}

CONFIG(release, debug|release): QMAKE_LIBDIR += $$PWD/release/
else:CONFIG(debug, debug|release): QMAKE_LIBDIR += $$PWD/debug/

#------------------------------------------------------------------------------
SOURCES += \
    main.cpp\
    mainwindow.cpp \
    FilmScan.cpp \
    project.cpp \
    readframedpx.cpp \
    wav.cpp \
    openglwindow.cpp \
    frame_view_gl.cpp \
    writexml.cpp \
    readframetiff.cpp \
    savesampledialog.cpp \
    preferencesdialog.cpp \
    extractdialog.cpp \
    metadata.cpp \
    videoencoder.cpp

HEADERS  += mainwindow.h \
    FilmScan.h \
    overlap.h \
    project.h \
    readframedpx.h \
    DPX.h \
    DPXHeader.h \
    DPXStream.h \
    wav.h \
    openglwindow.h \
    frame_view_gl.h \
    aeoexception.h \
    writexml.h \
    readframetiff.h \
    savesampledialog.h \
    preferencesdialog.h \
    extractdialog.h \
    metadata.h \
    videoencoder.h

FORMS    += mainwindow.ui \
    savesampledialog.ui \
    preferencesdialog.ui \
    extractdialog.ui

# locate the dpx library
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/release/
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/debug/
else:unix: LIBS += -L$$PWD/

LIBS += -ldpx

win32: LIBS += -llibtiff
else: LIBS += -ltiff

#win32-g++:CONFIG(release, debug|release) {
#	PRE_TARGETDEPS += $$PWD/./release/libdpx.a
#} else:win32-g++:CONFIG(debug, debug|release) {
#	PRE_TARGETDEPS += $$PWD/./debug/zlibdpx.a
#} else:win32:CONFIG(release, debug|release) {
#	PRE_TARGETDEPS += $$PWD/./release/dpx.lib
#} else:win32:CONFIG(debug, debug|release) {
#	PRE_TARGETDEPS += $$PWD/./debug/dpx.lib
#} else:unix {
#	PRE_TARGETDEPS += $$PWD/./libdpx.a
#}

unix: CONFIG += link_pkgconfig

# libav libraries
LIBS += -lavcodec -lavfilter -lavformat -lavutil
LIBS += -lswscale -lswresample

# OpenEXR libraries
# LIBS += -lImath -lHalf -lIex -lIexMath -lIlmThread -lIlmImf

# other libraries
LIBS += -ldspfilters

## Turn off unecessary warnings
unix: QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-private-field \
    -Wno-unused-variable -Wno-unused-parameter \
    -Wno-ignored-qualifiers -Wno-unused-function -Wno-sign-compare \
    -Wno-unused-local-typedef -Wno-reserved-user-defined-literal

RESOURCES += \
    shaders.qrc \
    license.qrc \
    images.qrc

DISTFILES +=
