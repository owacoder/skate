TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES +=

HEADERS += \
    containers/MFC/mfc_abstract_list.h \
    containers/POCO/poco_abstract_list.h \
    containers/Qt/qt_abstract_list.h \
    containers/WTL/wtl_abstract_list.h \
    containers/abstract_list.h \
    containers/abstract_map.h \
    containers/sparse_array.h \
    containers/split_join.h \
    containers/utf.h \
    io/adapters/base64.h \
    io/adapters/csv.h \
    containers/tree.h \
    io/adapters/md5.h \
    io/adapters/msgpack.h \
    io/buffer.h \
    io/logger.h \
    math/safeint.h \
    socket/iocp.h \
    socket/protocol/http.h \
    socket/wsaasyncselect.h \
    system/benchmark.h \
    system/time.h \
    threadbuffer.h \
    system/includes.h \
    system/environment.h \
    socket/common.h \
    socket/address.h \
    socket/socket.h \
    socket/select.h \
    socket/poll.h \
    socket/server.h \
    socket/kqueue.h \
    socket/epoll.h \
    io/adapters/json.h \
    io/adapters/core.h \
    io/adapters/xml.h

linux {
    LIBS += -lpthread
}

win32 {
    LIBS += -lWs2_32
}

DISTFILES += \
    todo.txt
