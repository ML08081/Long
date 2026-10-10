QT += core
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = qt_remote_protocol
INCLUDEPATH += ..
SOURCES += ../protocol.cpp ../../tests/local_validation/qt_remote_protocol.cpp
HEADERS += ../protocol.h
