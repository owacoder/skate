TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp

HEADERS += \
    common.h \
    socket.h \
    socket_address.h \
    system_includes.h \
    threadbuffer.h \
    environment.h \
    select.h \
    poll.h

linux {
    LIBS += -lpthread
}

win32 {
    LIBS += -lWs2_32
}
