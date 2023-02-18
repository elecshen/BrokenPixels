QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle

SOURCES += \
        main.cpp

#libtif
include(C:/Qt/5.15.2/Src/qtimageformats/src/3rdparty/libtiff.pri)
win32-g++:
{
        LIBS += -lz
}
win32-msvc*
{
        HEADERS += C:/Qt/5.15.2/msvc2015_64/include/QtZlib/zlib.h
}
