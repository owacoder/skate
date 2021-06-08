#include <iostream>
#include <threadbuffer.h>
#include <thread>
#include <vector>

#include "io/buffer.h"
#include "containers/tree.h"

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

#include <containers/abstract_list.h>
#include <forward_list>

int main()
{
#if 0
    dynamic_tree<std::string, false> t, copy;
    dynamic_tree<std::string, false>::iterator it = t.root();

    t.value() = "Value";

    it = t.replace(it, "Value");
    it = t.add_child(t.add_child(it, "Left"), {});
    t.add_child(it, "Right");
    copy = t;

    for (auto it = t.postorder_begin(); it.exists(); ++it)
        std::cout << *it << std::endl;

    std::cout << "  Height: " << t.height() << std::endl;
    std::cout << "    Size: " << t.size() << std::endl;
    std::cout << "  Leaves: " << t.leaf_count() << std::endl;
    std::cout << "Branches: " << t.branch_count() << std::endl;
#endif

#if 0
    std::array<char, 16> arr;
    std::vector<char> vec;
    std::list<char> lst = {'A', 'B', 'C'}, slst;
    std::set<char> set;
    std::string test_string;
    std::pair<char, int> pair;
    std::tuple<char, char> tuple;

    // Skate::AbstractList(pair) = Skate::AbstractList("JKL");
    Skate::AbstractList(slst) += Skate::AbstractList("123");
    Skate::AbstractList(test_string) = Skate::AbstractList(vec) = Skate::AbstractList(lst);
    Skate::AbstractList(test_string) += Skate::AbstractList(test_string);

    Skate::AbstractList(vec) = Skate::AbstractList(test_string);
    Skate::AbstractList(arr) = Skate::AbstractList("123");
    Skate::AbstractList(set) = Skate::AbstractList({'!', '@', '#'});
    Skate::AbstractList(test_string) += Skate::AbstractList(set);

    std::cout << "   arr: ";
    for (const auto &element : arr) {
        std::cout << (element ? element: '.');
    }
    std::cout << "|" << std::endl;

    std::cout << "   set: ";
    for (const auto &element : set) {
        std::cout << element;
    }
    std::cout << std::endl;

    std::cout << "   lst: ";
    for (const auto &element : lst) {
        std::cout << element;
    }
    std::cout << std::endl;

    std::cout << "  slst: ";
    for (const auto &element : slst) {
        std::cout << element;
    }
    std::cout << std::endl;

    std::cout << "vector: ";
    for (const auto &element : vec) {
        std::cout << element;
    }
    std::cout << std::endl;

    std::cout << "string: " << test_string << std::endl;

    return 0;
#endif

    try {
        Skate::StartupWrapper wrapper;
        Skate::TCPSocket socket;
        Skate::UDPSocket udp;
        Skate::SocketServer<Skate::Poll> server;

#if 0
        udp.bind(Skate::SocketAddress::any(), 8080);
        udp.connect("192.168.1.255", 80);
        udp.write("");
        udp.write_datagram("255.255.255.255", 80, "Test");

        return 0;
#endif

        socket.set_blocking(false);
        socket.bind(Skate::SocketAddress::any(), 8089);
        server.listen(socket, [](Skate::Socket *connection) {
            std::cout << "New connection to " << connection->remote_address().to_string(connection->remote_port()) <<
                         " is " << (connection->is_blocking()? "blocking": "nonblocking") << std::endl;
            return Skate::WatchRead;
        }, [](Skate::Socket *connection, Skate::WatchFlags watching, Skate::WatchFlags event) -> Skate::WatchFlags {
            if (connection->is_blocking()) {
                // Special handling for blocking sockets
            } else {
                // Special handling for nonblocking sockets
                if (event & Skate::WatchRead) {
                    connection->read_all(connection->read_buffer());
                    std::cout << connection->read_buffer().read_all<std::string>() << std::endl;
                } else if (event & Skate::WatchWrite) {
                    std::cout << "Writing to " << connection->remote_address().to_string(connection->remote_port()) << std::endl;
                    connection->write("HTTP/1.1 200 OK\r\n\r\nTest");
                    return 0;
                }
            }

            return watching;
        });
        server.run();

        socket.disconnect();
        socket.connect("owacoder.com", 80);
        socket.write("GET / HTTP/1.1\r\n"
                     "Host: owacoder.com\r\n"
                     "Connection: close\r\n\r\n");
        std::cout << socket.local_address().to_string(socket.local_port()) << std::endl;
        std::cout << socket.remote_address().to_string(socket.remote_port()) << std::endl;
        std::cout << socket.read_all<std::string>() << std::endl;

        std::cout << "Socket was bound\n";
        return 0;
    } catch (const std::exception &e) {
        std::cout << "ERROR: " << e.what() << std::endl;
        return 0;
    }

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
