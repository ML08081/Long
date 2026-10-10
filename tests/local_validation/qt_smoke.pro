QT += core gui widgets network testlib
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = qt_smoke
INCLUDEPATH += ../../LongLook
SOURCES += qt_smoke.cpp ../../LongLook/mainwindow.cpp ../../LongLook/netclient.cpp ../../LongLook/remotepanel.cpp ../../LongLook/imageview.cpp ../../LongLook/card.cpp ../../LongLook/joystickreader.cpp ../../LongLook/visionprocessor.cpp
HEADERS += ../../LongLook/mainwindow.h ../../LongLook/netclient.h ../../LongLook/remotepanel.h ../../LongLook/imageview.h ../../LongLook/card.h ../../LongLook/joystickreader.h ../../LongLook/visionprocessor.h
win32: LIBS += -lXinput -ldwmapi
