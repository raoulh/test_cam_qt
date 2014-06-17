#-------------------------------------------------
#
# Project created by QtCreator 2014-06-05T13:58:17
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = test_cam_qt
TEMPLATE = app

CONFIG += c++11

SOURCES += main.cpp\
        MainWindow.cpp \
    CamView.cpp

HEADERS  += MainWindow.h \
    CamView.h

FORMS    += MainWindow.ui
