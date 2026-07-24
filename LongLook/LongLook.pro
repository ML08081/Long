#-------------------------------------------------
# LongLook — 龙芯 2K0300 WiFi/TCP 前端 (上位机)
#   接收龙芯打包上来的传感器数据 + 视频 + 热成像并显示
#   仅依赖 QtWidgets + QtNetwork，跨平台（Windows 验证 / Linux 部署）
#-------------------------------------------------
QT       += core gui widgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET   = LongLook
TEMPLATE = app
CONFIG  += c++20
win32: LIBS += -lXinput

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    netclient.cpp \
    imageview.cpp \
    card.cpp \
    remotepanel.cpp \
    joystickreader.cpp \
    visionprocessor.cpp

HEADERS += \
    mainwindow.h \
    netclient.h \
    imageview.h \
    card.h \
    protocol.h \
    remotepanel.h \
    joystickreader.h \
    visionprocessor.h

# 视觉边车脚本随构建复制到可执行目录（VisionProcessor 会在此查找）
DISTFILES += yolo_sidecar.py
