TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp

HEADERS += \
    containers/MFC/mfc_abstract_list.h \
    containers/POCO/poco_abstract_list.h \
    containers/Qt/qt_abstract_list.h \
    containers/WTL/wtl_abstract_list.h \
    containers/abstract_list.h \
    containers/abstract_map.h \
    containers/adapters/adapters.h \
    containers/adapters/utf.h \
    containers/tree.h \
    io/buffer.h \
    socket/iocp.h \
    socket/wsaasyncselect.h \
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
