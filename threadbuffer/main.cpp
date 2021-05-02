#include <iostream>
#include <threadbuffer.h>
#include <thread>
#include <vector>

#include "socket/socket.h"
#include "socket/server.h"

static std::mutex coutMutex;
static int total;

template<typename Message>
void consumer(Skate::MessageHandler<Message> buffer) {
    Message m;

    buffer.send({});

    while (buffer.read(m)) {
        std::unique_lock<std::mutex> lock(coutMutex);
        std::cout << m[0] << std::endl;
    }
}

class MoveOnlyString {
    MoveOnlyString(const MoveOnlyString &) {}
    MoveOnlyString &operator=(const MoveOnlyString &) = default;

    std::string v;

public:
    MoveOnlyString() : v() {}
    MoveOnlyString(std::string &&str) : v(std::move(str)) {}
    MoveOnlyString(MoveOnlyString &&other) {v = std::move(other.v);}

    MoveOnlyString &operator=(MoveOnlyString &&other) = default;

    const char &operator[](size_t idx) const {return v[idx];}
    char &operator[](size_t idx) {return v[idx];}
};

std::ostream &operator<<(std::ostream &os, const Skate::SocketAddress &address) {
    return os << address.to_string();
}

template<template<typename T, typename A> class Container, typename T, typename A>
std::ostream &operator<<(std::ostream &os, const Container<T, A> &container) {
    for (const auto &item: container)
        os << item << " Loopback? " << item.is_loopback() << "\n";
    return os;
}

int main()
{
    try {
        Skate::StartupWrapper wrapper;
        Skate::TCPSocket socket;
        Skate::SocketServer<Skate::Select> server;

        socket.bind(Skate::SocketAddress::any(), 8089);
        server.listen(socket, [](Skate::Socket *connection) {
            std::cout << "New connection to " << connection->remote_address().to_string(connection->remote_port()) << std::endl;
        }, [](Skate::Socket *connection, Skate::WatchFlags flags) {
            if (flags & Skate::WatchWrite) {
                std::cout << "Writing to " << connection->remote_address().to_string(connection->remote_port()) << std::endl;
                connection->write("HTTP/1.1 200 OK\r\n\r\nTest");
                connection->disconnect();
            }
        });
        //server.run();

        //return 0;

        socket.disconnect();
        socket.connect("www.google.com", 80);
        socket.write("GET / HTTP/1.1\r\n"
                     "Host: www.google.com\r\n"
                     "Connection: close\r\n\r\n");
        std::cout << socket.local_address().to_string(socket.local_port()) << std::endl;
        std::cout << socket.remote_address().to_string(socket.remote_port()) << std::endl;
        std::cout << socket.read_all() << std::endl;

        std::cout << "Socket was bound\n";
        return 0;
    } catch (const std::exception &e) {
        std::cout << "ERROR: " << e.what() << std::endl;
        return 0;
    }

    Skate::Poll poll;
    Skate::Select select;

    poll.poll([](Skate::SocketDescriptor, Skate::WatchFlags) {});

    typedef std::string Message;
    std::unique_ptr<Skate::MessageBroadcaster<Message>> msg(new Skate::MessageBroadcaster<Message>());

    std::thread thrd(consumer<Message>, msg->addBuffer());
    //std::thread thrd2(consumer<int>, 2, std::ref(tbuf));
    
    msg->addFileOutput("F:/Scratch/test.txt");
    //msg->addAsyncCallback([](const Message &m) {std::this_thread::sleep_for(std::chrono::milliseconds(1500)); {std::unique_lock<std::mutex> lock(coutMutex); std::cout << m[0] << "!" << std::endl;}}, 4);
    auto ptr = msg->addCallback([](const Message &m) {std::this_thread::sleep_for(std::chrono::milliseconds(1500)); {std::unique_lock<std::mutex> lock(coutMutex); std::cout << m[0] << "!" << std::endl; return true;}}, 4);
    msg->addCallback([](Message &&m) {std::unique_lock<std::mutex> lock(coutMutex); std::cout << "Sync callback: " << m[0] << std::endl; return true;});
    msg->send(std::string("abc"), Skate::QueueBlockUntilDone);
    msg->send(std::string("def"), Skate::QueueBlockUntilDone);
    msg->send(std::string("jkl"));
    msg->send(std::string("ghi"));
    //std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    //msg->sendMessages({{"mno"}, {"pqr"}, {"stu"}, {"vwx"}});
    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    msg->close();
    msg = nullptr;
    
    {
        std::unique_lock<std::mutex> lock(coutMutex);
        std::cout << "ALL MESSAGES SENT" << std::endl;
    }
    
    thrd.join();
    //thrd2.join();
    
    std::cout << "Hello World! " << total << std::endl;
    return 0;
}
