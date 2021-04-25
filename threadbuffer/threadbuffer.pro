TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp

HEADERS += \
    common.h \
    io/device.h \
    socket/address.h \
    socket/socket.h \
    system/includes.h \
    threadbuffer.h \
    system/environment.h \
    socket/select.h \
    socket/poll.h

linux {
    LIBS += -lpthread
}

win32 {
    LIBS += -lWs2_32
}
