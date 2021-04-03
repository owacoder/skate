TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp \
        threadbuffer.cpp \
    select.cpp

HEADERS += \
    threadbuffer.h \
    environment.h \
    select.h \
    iodevice.h \
    poll.h

linux {
    LIBS += -lpthread
}
