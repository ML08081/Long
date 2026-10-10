# 串口直连为主通道（QtSerialPort 跨平台，支持龙芯 2K0300/Linux）。
# 注：BLE 已弃用、不再接入 UI。龙芯构建若 Qt 不含 QtBluetooth，
#     可从下面删除 bluetooth 模块、并从 SOURCES/HEADERS 移除 bledevicemanager.*。
QT       += core gui serialport network bluetooth widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = RemoteController
TEMPLATE = app

CONFIG += c++11

# XInput 手柄仅 Windows 可用；龙芯/Linux 上用模拟手柄+键盘控制（不链接 Xinput）
win32: LIBS += -lXinput

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    protocol.cpp \
    joystickreader.cpp \
    bluetoothmanager.cpp \
    bledevicemanager.cpp \
    wifimanager.cpp

HEADERS += \
    mainwindow.h \
    protocol.h \
    config.h \
    joystickreader.h \
    bluetoothmanager.h \
    bledevicemanager.h \
    wifimanager.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources.qrc
