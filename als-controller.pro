TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    comsock.cpp

HEADERS += \
    comsock.h

LIBS += -pthread
