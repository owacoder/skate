#include <iostream>
#include <threadbuffer.h>
#include <thread>
#include <vector>

#include "socket.h"
#include "poll.h"
#include "select.h"

static std::mutex coutMutex;
static int total;

template<typename Message>
void consumer(std::shared_ptr<MessageBuffer<Message>> buffer) {
    Message m;
    while (buffer->read(m)) {
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
    return os << address.to_string(true);
}

std::ostream &operator<<(std::ostream &os, const Skate::Socket::AddressInfo &address) {
    return os << address.address.to_string(true);
}

template<template<typename T, typename A> class Container, typename T, typename A>
std::ostream &operator<<(std::ostream &os, const Container<T, A> &container) {
    for (const auto &item: container)
        os << item << "\n";
    return os;
}

int main()
{
    try {
        Skate::StartupWrapper wrapper;
        Skate::TCPSocket socket;

        std::cout << Skate::TCPSocket().remote_server_addresses(Skate::SocketAddress("localhost", 80, Skate::SocketAddress::IPAddressUnspecified)) << std::endl;
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    return 0;

    Skate::Poll poll;
    Skate::Select select;

    poll.poll([](Skate::SocketDescriptor, Skate::WatchFlags) {});

    typedef MoveOnlyString Message;
    std::unique_ptr<MessageBroadcaster<Message>> msg(new MessageBroadcaster<Message>());

    std::thread thrd(consumer<Message>, msg->addBuffer());
    //std::thread thrd2(consumer<int>, 2, std::ref(tbuf));
    
    // msg->addAsyncFileOutput("F:/Scratch/test.txt");
    msg->addAsyncCallback([](const Message &m) {std::this_thread::sleep_for(std::chrono::milliseconds(1500)); {std::unique_lock<std::mutex> lock(coutMutex); std::cout << m[0] << "!" << std::endl;}}, 4);
    auto ptr = msg->addAsyncCallback([](const Message &m) {std::this_thread::sleep_for(std::chrono::milliseconds(1500)); {std::unique_lock<std::mutex> lock(coutMutex); std::cout << m[0] << "!" << std::endl;}}, 4);
    ptr->send(std::string("abc"));
    ptr->send(std::string("def"));
    ptr->send(std::string("jkl"));
    ptr->send(std::string("ghi"));
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    //msg->sendMessages({{"mno"}, {"pqr"}, {"stu"}, {"vwx"}});
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    msg->close();
    // msg = nullptr;
    
    {
        std::unique_lock<std::mutex> lock(coutMutex);
        std::cout << "ALL MESSAGES SENT" << std::endl;
    }
    
    thrd.join();
    //thrd2.join();
    
    std::cout << "Hello World! " << total << std::endl;
    return 0;
}
