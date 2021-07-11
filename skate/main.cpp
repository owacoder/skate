#include <iostream>
#include "threadbuffer.h"
#include <thread>
#include <vector>

#include "io/buffer.h"
#include "containers/tree.h"

#include "socket/socket.h"
#include "socket/server.h"

static std::mutex coutMutex;
static int total;

template<typename Message>
void consumer(skate::MessageHandler<Message> buffer) {
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

std::ostream &operator<<(std::ostream &os, const skate::SocketAddress &address) {
    return os << address.to_string();
}

//#include "containers/abstract_map.h"
#if 0
#include <QString>
#include <QList>
#include <QMap>
#endif

#include "containers/adapters/core.h"
#include "containers/adapters/json.h"
#include "containers/adapters/csv.h"
#include "containers/adapters/xml.h"
#include <map>
#include "benchmark.h"

struct Point {
    int x, y;
    Point() : x(), y() {}
};

template<typename StreamChar>
void skate_json(std::basic_istream<StreamChar> &is, Point &p) {

}

template<typename StreamChar>
bool skate_json(std::basic_streambuf<StreamChar> &os, const Point &p, skate::json_write_options options) {
    return skate::json(std::vector<int>{{p.x, p.y}}, options).write(os);
}

#include <string_view>

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

    std::vector<std::string> v = {"A\nnewline'\"", " 1", "2   ", "3"};
    std::unordered_map<std::string, skate::json_value> map;

    // map["default\xf0\x9f\x8c\x8d"] = {};
    // map["test"] = {};

    //std::cout << typeid(decltype(map)).name() << std::endl;

#if 1
    std::istringstream jstream("{\"string\xf0\x9f\x8c\x8d\":[\"string\\ud83c\\udf0d\", 1000, null, true]}");
    for (skate::unicode_codepoint cp = {0xd83c, 0xdf0d}; jstream >> cp; ) {
        std::cout << "Codepoint: " << cp.character() << '\n';
    }

    std::cout << '\n';

    skate::json_value js;
    std::string js_text;

    skate::benchmark([&js]() {
        js.resize(200000);
        for (size_t i = 0; i < js.size(); ++i) {
            skate::json_value temp;

            temp["1st"] = rand();
            temp["2nd"] = true;//rand() * 0.00000000000001;
            temp["3rd"] = std::string(10, 'A') + std::string("\xf0\x9f\x8c\x8d") + std::to_string(rand());
            temp[L"4th" + std::wstring(1, wchar_t(0xd83c)) + wchar_t(0xdf0d)] = L"Wide" + std::wstring(1, wchar_t(0xd83c)) + wchar_t(0xdf0d);

            js[i] = std::move(temp);
        }
    }, "JSON build");

    const size_t count = 1;
    for (size_t i = 0; i < count; ++i) {
        skate::benchmark_throughput([&js_text, &js]() {
            js_text = skate::to_json(js);
            return js_text.size();
        }, "JSON write " + std::to_string(i));
    }

    std::cout << js_text.substr(0, 1000) << '\n';

    for (size_t i = 0; i < count; ++i) {
        skate::benchmark_throughput([&js_text, &js]() {
            js = skate::from_json<typename std::remove_reference<decltype(js)>::type>(js_text);
            return js_text.size();
        }, "JSON read " + std::to_string(i));
    }

    std::cout << js_text.size() << std::endl;

    jstream.clear();
    jstream.seekg(0);
    if (jstream >> skate::json(map))
        std::cout << skate::json(map) << '\n';
    else
        std::cout << "An error occurred\n";

    std::cout << skate::json(Point(), { 2 });
#endif

    //skate::put_unicode<char>{}(std::cout, 127757);
    //std::cout << Skate::impl::is_map<int>::value << std::endl;
    //std::cout << Skate::impl::is_map<typename std::remove_cv<decltype(map)>::type>::value << std::endl;
    //std::cout << Skate::impl::is_map<std::map<int, int>>::value << std::endl;

    //std::wcout << Skate::json(map) << std::endl;
    //std::cout << Skate::json(map, 2) << Skate::json(true) << std::endl;
    //std::cout << skate::csv(v, {',', '"'}) << std::endl;
    std::map<std::string, std::string> xmap;

    xmap["st-1"] = "Test 1";
    xmap["st-2"] = "Test 2: <> or \" will be escaped, along with '";

    std::cout << skate::xml_doc(xmap, 1) << std::endl;
    //std::cout << Skate::json(v) << Skate::json(nullptr) << std::endl;

    std::string narrow = skate::utf_convert<std::string>(L"Wide to narrow string");
    std::wstring wide = skate::utf_convert<std::wstring>(std::string_view("Narrow to wide string\xf0\x9f\x8c\x8d"));

    for (const auto c : narrow) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << int(c & 0xff) << ' ';
    }
    std::cout << '\n';

    for (const auto wc : wide) {
        std::cout << std::hex << std::setfill('0') << std::setw(4) << int(wc) << ' ';
    }
    std::cout << '\n';

    std::vector<std::map<std::string, std::string>> csv;

    csv.push_back({{ "Header 1", "Sample 1" }, {"Fruit", "Orange"}});
    csv.push_back({{ "Header 2", "Sample 2" }, {"Fruit", "Apple"}});
    csv.push_back({{ "Header 2", "Sample 3" }, {"Fruit", "Banana"}});
    csv.push_back({{ "Header 1", "Sample 4" }, {"Fruit", "Strawberry"}});
    csv.push_back({{ "Header 3", "Sample 5" }, {"Fruit", "Blueberry"}});

    std::map<std::string, std::string> cmap;

    cmap["Header 1"] = "Sample 1";
    cmap["Header 2"] = "Sample 2";
    cmap[""] = "Test";

    std::map<std::string, std::vector<std::string>> cvec;

    cvec["Header 1"] = {"Row 1", "Row 2", "Row 3", "Row 4"};
    cvec["Header 2"] = {"Item"};
    cvec["Header 3"] = {"Orange", "Apple", "Banana", "Kiwi", "Mango"};

    std::cout << skate::csv(csv) << '\n';
    std::cout << skate::csv(cmap) << '\n';
    std::cout << skate::csv(cvec) << '\n';

    std::istringstream icsv("1,-1,0.01\r333440,-3,44000,0,0\r");
    std::map<std::string, double> csvline;

    skate::csv_options opts(',', '"', false);
    if (icsv >> skate::csv(csvline, opts))
        std::cout << "SUCCESS: " << skate::json(csvline) << '\n' << skate::csv(csvline, opts) << '\n';
    else
        std::cout << "Failed\n";

    std::array<std::string, 10> array;

    std::cout << skate::csv(std::make_tuple(std::string("std::string"), double(3.14159265), true, -1, 0, nullptr, skate::is_trivial_tuple_base<decltype(std::make_tuple(std::string{}, 0))>::value));
    std::cout << skate::json(array);

    return 0;

#if 0
    std::array<char, 16> arr;
    std::vector<int> vec;
    std::list<char> lst = {'A', 'B', 'C'};
    std::forward_list<char> slst;
    std::set<char> set;
    std::string test_string;
    std::pair<char, int> pair;
    std::pair<char, char> pair2;
    std::tuple<char, char, int> tuple;
    std::bitset<4> bitset;
    std::valarray<int> valarray;
    std::map<std::string, std::string> map;
    std::multimap<std::string, std::string> mmap;

    auto abstract_map = Skate::AbstractMap(map);
    abstract_map["A"] = "1";
    abstract_map["B"] = "2";
    abstract_map["C"] = "3";
    Skate::AbstractMap(mmap) += abstract_map;

    Skate::AbstractMap(mmap).values().apply([](const auto &el) {std::cout << "Map element: " << el << std::endl;});

    bitset[0] = false;
    bitset[1] = true;
    bitset[2] = true;
    bitset[3] = true;

    // TODO:
    // pair = tuple
    // tuple = pair
    // pair = other pair
    // tuple = other tuple

    Skate::AbstractList(pair) = Skate::AbstractList("JKL");
    Skate::AbstractList(tuple) = Skate::AbstractList("JKL");
    Skate::AbstractList(vec) = Skate::AbstractList(tuple);
    //Skate::AbstractList(slst) = Skate::AbstractList("Wahoo!") + Skate::AbstractList(tuple);
    Skate::AbstractList(slst) += Skate::AbstractList(tuple);
    Skate::AbstractList(vec) = Skate::AbstractList(pair);
    Skate::AbstractList(test_string) = Skate::AbstractList(lst);
    Skate::AbstractList(test_string) += Skate::AbstractList(test_string);

    Skate::AbstractList(vec) = Skate::AbstractList(test_string);
    Skate::AbstractList(arr) = Skate::AbstractList(tuple);
    Skate::AbstractList(set) = Skate::AbstractList({'!', '@', '#'});
    Skate::AbstractList(set) += Skate::AbstractList(tuple);
    Skate::AbstractList(test_string) += Skate::AbstractList(set);
    Skate::AbstractList(vec) = Skate::AbstractList(bitset);
    Skate::AbstractList(bitset) = Skate::AbstractList(vec);
    Skate::AbstractList(valarray) << Skate::AbstractList(vec);
    //Skate::AbstractList(vec) = Skate::AbstractList(valarray);

    std::cout << "  pair: " << pair.first << "," << pair.second << "\n";

    std::cout << " tuple: ";
    Skate::AbstractList(tuple).apply([](const auto &el) { std::cout << el << ",";});
    std::cout << std::endl;

    std::cout << "valarr: ";
    Skate::AbstractList(valarray).apply([](const int &el) {std::cout << el << ",";});
    std::cout << std::endl;

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
        skate::StartupWrapper wrapper;
        skate::TCPSocket socket;
        skate::UDPSocket udp;
        skate::SocketServer<skate::Poll> server;

#if 0
        udp.bind(Skate::SocketAddress::any(), 8080);
        udp.connect("192.168.1.255", 80);
        udp.write("");
        udp.write_datagram("255.255.255.255", 80, "Test");

        return 0;

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
#endif
    } catch (const std::exception &e) {
        std::cout << "ERROR: " << e.what() << std::endl;
        return 0;
    }

    typedef std::string Message;
    std::unique_ptr<skate::MessageBroadcaster<Message>> msg(new skate::MessageBroadcaster<Message>());

    std::thread thrd(consumer<Message>, msg->addBuffer());
    //std::thread thrd2(consumer<int>, 2, std::ref(tbuf));
    
    msg->addFileOutput("F:/Scratch/test.txt");
    //msg->addAsyncCallback([](const Message &m) {std::this_thread::sleep_for(std::chrono::milliseconds(1500)); {std::unique_lock<std::mutex> lock(coutMutex); std::cout << m[0] << "!" << std::endl;}}, 4);
    auto ptr = msg->addCallback([](const Message &m) {std::this_thread::sleep_for(std::chrono::milliseconds(1500)); {std::unique_lock<std::mutex> lock(coutMutex); std::cout << m[0] << "!" << std::endl; return true;}}, 4);
    msg->addCallback([](Message &&m) {std::unique_lock<std::mutex> lock(coutMutex); std::cout << "Sync callback: " << m[0] << std::endl; return true;});
    msg->send(std::string("abc"), skate::QueueBlockUntilDone);
    msg->send(std::string("def"), skate::QueueBlockUntilDone);
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
