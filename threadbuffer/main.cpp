#include <iostream>
#define NOMINMAX
#include <windows.h>

#include <threadbuffer.h>
#include <thread>
#include <vector>

static std::mutex coutMutex;
static int total;

template<typename Message>
void consumer(std::shared_ptr<MessageBuffer<Message>> buffer) {
    Message m;
    while (buffer->readMessage(m)) {
        std::unique_lock<std::mutex> lock(coutMutex);
        std::cout << m[0] << "\n";
    }
}

class MoveOnlyString {
    MoveOnlyString(const MoveOnlyString &) {}
    MoveOnlyString &operator=(const MoveOnlyString &) {}

    std::string v;

public:
    MoveOnlyString() : v() {}
    MoveOnlyString(std::string &&str) : v(std::move(str)) {}
    MoveOnlyString(MoveOnlyString &&other) {v = std::move(other.v);}

    MoveOnlyString &operator=(MoveOnlyString &&other) = default;

    const char &operator[](size_t idx) const {return v[idx];}
    char &operator[](size_t idx) {return v[idx];}
};

int main()
{
    typedef MoveOnlyString Message;
    std::unique_ptr<MessageBroadcaster<Message>> msg(new MessageBroadcaster<Message>());

    std::thread thrd(consumer<Message>, msg->addBuffer());
    //std::thread thrd2(consumer<int>, 2, std::ref(tbuf));
    
    // msg->addAsyncFileOutput("F:/Scratch/test.txt");
    msg->addAsyncCallback([](const Message &m) {Sleep(1500); {std::unique_lock<std::mutex> lock(coutMutex); std::cout << m[0] << "!\n";}}, 4);
    auto ptr = msg->addAsyncCallback([](const Message &m) {Sleep(1500); {std::unique_lock<std::mutex> lock(coutMutex); std::cout << m[0] << "!\n";}}, 4);
    ptr->sendMessage(std::string("abc"));
    ptr->sendMessage(std::string("def"));
    ptr->sendMessage(std::string("ghi"));
    ptr->sendMessage(std::string("jkl"));
    Sleep(4000);
    //msg->sendMessages({{"mno"}, {"pqr"}, {"stu"}, {"vwx"}});
    Sleep(1000);
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
