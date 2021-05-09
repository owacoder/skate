TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp

HEADERS += \
    threadbuffer.h \
    io/device.h \
    system/includes.h \
    system/environment.h \
    socket/common.h \
    socket/address.h \
    socket/socket.h \
    socket/select.h \
    socket/poll.h \
    socket/server.h \
    socket/kqueue.h \
    socket/epoll.h

linux {
    LIBS += -lpthread
}

win32 {
    LIBS += -lWs2_32
}
