QT += core network testlib
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = qt5_ping
isEmpty(DIRECT_CONTROL_DIR): DIRECT_CONTROL_DIR = $$PWD/../../遥控手柄V2
INCLUDEPATH += $$DIRECT_CONTROL_DIR
SOURCES += qt5_ping.cpp $$DIRECT_CONTROL_DIR/wifimanager.cpp
HEADERS += $$DIRECT_CONTROL_DIR/wifimanager.h
