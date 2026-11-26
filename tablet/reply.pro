TEMPLATE = lib
TARGET = reply
CONFIG += shared plugin no_plugin_name_prefix c++17
QT += core gui qml quick svg network
SOURCES += src/diary.cpp xovi.cpp
QMAKE_CXXFLAGS += -fPIC
QMAKE_LFLAGS += -Wl,-Bsymbolic -Wl,--version-script=$$PWD/reply.map

PRE_TARGETDEPS += $$PWD/reply.map
