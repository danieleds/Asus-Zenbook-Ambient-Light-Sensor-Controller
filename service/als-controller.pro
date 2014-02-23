TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    client.cpp \
    comsock.cpp

HEADERS += \
    comsock.h \
    client.h

LIBS += -pthread -lbsd
