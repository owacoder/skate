#define NOMINMAX
//#include <afx.h>

#include <iostream>
#include "threadbuffer.h"
#include <thread>
#include <vector>

#include "io/buffer.h"
#include "containers/tree.h"

#include "socket/socket.h"
#include "socket/server.h"

static std::mutex coutMutex;

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

    operator std::string() const { return v; }

    const char &operator[](size_t idx) const {return v[idx];}
    char &operator[](size_t idx) {return v[idx];}
};

std::ostream &operator<<(std::ostream &os, const skate::socket_address &address) {
    return os << address.to_string();
}

#include "io/streams.h"
#include "containers/abstract_list.h"
//#include "containers/MFC/mfc_abstract_list.h"
#if 0
#include <QString>
#include <QList>
#include <QMap>
#endif

#include "io/logger.h"
#include "io/adapters/core.h"
#include "io/adapters/json.h"
#include "io/adapters/msgpack.h"
#include "io/adapters/csv.h"
#include "io/adapters/xml.h"
#include "containers/split_join.h"
#include "system/time.h"
#include <map>
#include <array>
#include "system/benchmark.h"

void abstract_container_test() {
    std::initializer_list<int> il = { 0, 1, 2, 3, 4, 5 };

    std::vector<int> v = il;
    std::list<int> l = il;
    std::forward_list<int> fl = il;
    // CArray<int, int> ca;
    // CList<int, int> cl;
    // CString s;

    namespace abstract = skate::abstract;

    //for (const auto el : il) {
    //    abstract::push_back(ca, el);
    //    abstract::push_back(cl, el);
    //}

    abstract::push_back(v, 6);
    abstract::push_back(l, 6);
    abstract::push_back(fl, 6);
    //abstract::push_back(ca, 6);
    //abstract::push_back(cl, 6);
    abstract::pop_front(v);
    abstract::pop_front(l);
    abstract::pop_front(fl);
    //abstract::pop_front(ca);
    //abstract::pop_front(cl);
    abstract::reverse(v);
    abstract::reverse(l);
    abstract::reverse(fl);
    //abstract::reverse(ca);
    //abstract::reverse(cl);

    //for (const auto &el: "CString test")
    //    abstract::push_back(s, el);
    //abstract::element(s, 0) = 0;
    //abstract::reverse(s);

    std::cout << skate::json(v) << std::endl;
    std::cout << skate::json(l) << std::endl;
    std::cout << skate::json(fl) << std::endl;
    //std::cout << skate::json(ca) << std::endl;
    //std::cout << skate::json(cl) << std::endl;
    //std::cout << skate::json(s) << std::endl;

    std::cout << skate::json(abstract::element(v, 5)) << std::endl;
    std::cout << skate::json(abstract::element(l, 5)) << std::endl;
    std::cout << skate::json(abstract::element(fl, 5)) << std::endl;
    //std::cout << skate::json(abstract::element(ca, 5)) << std::endl;
    //std::cout << skate::json(abstract::element(cl, 5)) << std::endl;
    //std::cout << skate::json(abstract::back(s)) << std::endl;
}

#include "socket/protocol/http.h"

void network_test() {
    skate::startup_wrapper wrapper;
    skate::socket_server<> server;
    skate::http_client_socket http;
    std::error_code ec;

    skate::http_request req;
    req.url.set_hostname("territory.ddns.net");
    req.headers["Connection"] = "close";

    auto resolved = http.resolve(ec, { req.url.get_host(skate::url::encoding::raw), req.url.get_port(80) });
    http.set_blocking(ec, false);
    if (resolved.size())
        http.connect_sync(ec, resolved);

    auto save = ec;
    std::cout << skate::json(resolved) << save.message() << std::endl;
    http.write_http_request(ec, std::move(req));

    // http.listen(ec);
    std::cout << save.message() << " " << ec.message() << std::endl;
    server.serve_socket(&http);
    std::cout << "server running" << std::endl;
    server.run();
}

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

template<typename T>
void io_buffer_consumer(skate::io_threadsafe_pipe<T> pipe, size_t id) {
    skate::io_threadsafe_pipe_guard<T> guard(pipe);

    T data;
    while (pipe.read(data)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "Got data: " << id << ": " << skate::json(data) << std::endl;

        pipe.write("Feedback from " + std::to_string(id) + ": Got " + skate::to_json(data));
    }

    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << "Consumer hanging up" << std::endl;
}

template<typename T>
void io_buffer_producer(skate::io_threadsafe_pipe<T> pipe) {
    skate::io_threadsafe_pipe_guard<T> guard(pipe);

    for (size_t i = 0; i < 8; ++i) {
        pipe.write(std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        std::string feedback;
        while (pipe.read(feedback, false)) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << feedback << std::endl;
        }
    }
    guard.close_write();

    std::string feedback;
    while (pipe.read(feedback)) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << feedback << std::endl;
    }

    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << "Producer hanging up" << std::endl;
}

template<typename T>
constexpr int log10ceil(T num) {
    return num < 10? 1: 1 + log10ceil(num / 10);
}

namespace skate {
    template<typename StreamChar>
    bool skate_json(std::basic_streambuf<StreamChar> &os, const socket_address &a, const json_write_options &options) {
        return skate::json(a.to_string(), options).write(os);
    }
}

#include "math/safeint.h"

int main()
{
#if 0
    {
        skate::basic_safeint<unsigned> u = 0;
        skate::basic_safeint<int, skate::safeint_mode::error> i = INT_MAX;

        i += (long long) (INT_MAX) + 1;

        std::cout << i.value() << std::endl;

        return 0;
    }

    auto save = std::cout.rdbuf();
    skate::teebuf tee{save, std::cerr.rdbuf()};
    std::cout.rdbuf(&tee);

    std::cout << "Hello, World!";

    skate::cfilebuf cfile;
    cfile.open("F:\\Scratch\\test.txt", std::ios_base::in);
    std::cout.rdbuf(save);

    while (1) {
        const auto c = cfile.sbumpc();

        if (c == EOF)
            break;

        std::cout << char(c);
    }

    return 0;
#endif

#if 0
    if (0)
    {
        std::vector<std::string> v = {"A\nnewline'\"", " 1", "2   ", "3"};
        std::unordered_map<std::string, skate::json_value> map;

        std::istringstream jstream("{\"string\xf0\x9f\x8c\x8d\":[\"string\\ud83c\\udf0d\", 1000, null, true]}");
        for (skate::unicode_codepoint cp = {0xd83c, 0xdf0d}; jstream >> cp; ) {
            std::cout << "Codepoint: " << cp.character() << '\n';
        }

        std::cout << '\n';

        skate::json_value js;
        std::string js_text;

        const size_t count = 20;
        if (count) {
            skate::benchmark([&js]() {
                js.resize(200000);
                for (size_t i = 0; i < js.size(); ++i) {
                    skate::json_value temp;

                    temp["1st"] = rand();
                    temp["2nd"] = std::nullptr_t{}; //rand() * 0.00000000000001;
                    temp["3rd"] = std::string(10, 'A') + std::string("\xf0\x9f\x8c\x8d") + std::to_string(rand());
                    temp[L"4th" + std::wstring(1, wchar_t(0xd83c)) + wchar_t(0xdf0d)] = L"Wide" + std::wstring(1, wchar_t(0xd83c)) + wchar_t(0xdf0d);

                    js[i] = std::move(temp);
                }
            }, "JSON build");
        }

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
    }

    if (0)
    {
        skate::msgpack_value ms;
        std::string ms_text;

        const size_t mcount = 20;
        if (mcount) {
            skate::benchmark([&ms]() {
                ms.resize(200000);
                for (size_t i = 0; i < ms.size(); ++i) {
                    skate::msgpack_value temp;

                    temp[skate::msgpack_value(0)] = rand();
                    temp[skate::msgpack_value(1)] = rand() * 0.00000000000001;
                    temp[skate::msgpack_value(2)] = std::string(10, 'A') + std::string("\xf0\x9f\x8c\x8d") + std::to_string(rand());
                    temp[skate::msgpack_value(3)] = L"Wide" + std::wstring(1, wchar_t(0xd83c)) + wchar_t(0xdf0d);

                    ms[i] = std::move(temp);
                }
            }, "MsgPack build");
        }

        for (size_t i = 0; i < mcount; ++i) {
            skate::benchmark_throughput([&ms_text, &ms]() {
                ms_text = skate::to_msgpack(ms);
                return ms_text.size();
            }, "MsgPack write " + std::to_string(i));
        }

        std::cout << ms_text.substr(0, 1000) << '\n';

        for (size_t i = 0; i < mcount; ++i) {
            skate::benchmark_throughput([&ms_text, &ms]() {
                ms = skate::from_msgpack<typename std::remove_reference<decltype(ms)>::type>(ms_text);
                return ms_text.size();
            }, "MsgPack read " + std::to_string(i));
        }

        std::cout << ms_text.size() << std::endl;
    }

    network_test();
    return 0;
#endif

#if 0
    abstract_container_test();
    return 0;
#endif

#if 0
    std::string text = "1, no";

    std::cout << skate::from_csv<bool>(text) << std::endl;
    const auto msgp = skate::to_msgpack(text);

    for (const auto &el: msgp)
        std::cout << skate::toxchar(el >> 4) << skate::toxchar(el & 0xf) << ' ';
    std::cout << '\n';

    return 0;
#endif

#if 0
    {
#if WINDOWS_OS
        const char *path = "F:/Scratch/test.log";
#else
        const char *path = "/shared/Scratch/test.log";
#endif
        skate::async_file_logger datalog(path, skate::default_logger_options{skate::time_point_string_options::default_enabled()}, std::ios_base::out);
        //datalog.set_buffer_limit(0);

        skate::benchmark([&]() {
            std::thread thrd[10];

            for (size_t i = 0; i < 10; ++i) {
                thrd[i] = std::thread([&, i]() {
                    for (size_t j = 0; j < 10000000; ++j) {
                        datalog.info("Thread " + std::to_string(i) + ", Message " + std::to_string(j));
                    }
                });
            }

            for (size_t i = 0; i < 10; ++i)
                thrd[i].join();
        });

        skate::benchmark([&]() {
            skate::benchmark_throughput([&]() {
                for (size_t i = 0; i < 10000000; ++i) {
                    datalog.info("Message " + std::to_string(i));
                }

                return 10000000;
            }, "Logger");

            datalog.close();
        }, "Entire logger");
    }

    {
        const std::string info = "INFO: ";
        const std::string data = "Some random info";

        std::ostringstream out;
        auto &buf = *out.rdbuf();
        skate::benchmark([&]() {
            for (size_t i = 0; i < 1000; ++i) {
                buf.sputn(info.c_str(), info.size());
                buf.sputn(data.c_str(), data.size());
                buf.sputc('\n');
            }
        }, "Raw");
    }

    return 0;

    std::cout << skate::compare_nocase_ascii("textA", "TEXTa") << std::endl;
#endif

    network_test();

    return 0;

    std::cout << skate::json(skate::split<std::vector<std::string>>("Header, Test,,, None 2", ",", true)) << std::endl;
    std::cout << skate::json(skate::join(std::vector<std::string>{"Header", "Data", "Data", "", "Data"}, ",")) << std::endl;

    skate::url u;

    u.set_scheme("http");
    u.set_hostname("::::");
    u.set_username("owacoder");
    u.set_path("/path to data/to/data");
    u.set_password("password:");
    u.set_fragment("fragment#");
    u.set_query("key", "value");
    u.set_query("test", "%^&#@@");

    std::cout << u.to_string() << std::endl;
    std::cout << skate::network_address("192.168.1.1:80").to_string() << std::endl;
    std::cout << skate::network_address("192.168.1.1:").to_string() << std::endl;
    std::cout << skate::network_address("owacoder--cnnxnns:80").to_string(false) << std::endl;
    std::cout << skate::network_address("0.0.0.0:8000").with_port(6500).to_string() << std::endl;
    std::cout << skate::network_address("[::1]").to_string() << std::endl;
    std::cout << skate::network_address("[::]:80").to_string() << std::endl;

    return 0;

    typedef float FloatType;

    char buf[512];
    snprintf(buf, sizeof(buf), "%.*g", std::numeric_limits<FloatType>::max_digits10, 1e8);
    printf("%u: %s\n", (unsigned) strlen(buf), buf);
    printf("e: %u\n", std::numeric_limits<FloatType>::max_digits10 + 4 + log10ceil(std::numeric_limits<FloatType>::max_exponent10));
    printf("f: %u\n", 2 + std::numeric_limits<FloatType>::max_exponent10 + std::numeric_limits<FloatType>::max_digits10);

    skate::io_buffer<MoveOnlyString> b(3);

    b.write(MoveOnlyString("1"));
    b.write(MoveOnlyString("2"));
    b.write(MoveOnlyString("3"));
    b.write(MoveOnlyString("4"));
    std::cout << skate::json(b.read<std::vector<std::string>>(4)) << std::endl;
    b.write(MoveOnlyString("4"));
    std::cout << skate::json(b.read<std::vector<std::string>>(3)) << std::endl;

    std::cout << "Interfaces: " << skate::json(skate::socket_address::interfaces(), skate::json_write_options(2)) << std::endl;

#if 0
    auto buffer = skate::make_threadsafe_pipe<std::string>(3);

    std::thread thrd1(io_buffer_consumer<std::string>, buffer.first, 1);
    std::thread thrd2(io_buffer_consumer<std::string>, buffer.first, 2);
    std::thread thrd0(io_buffer_producer<std::string>, buffer.second);

    thrd0.join();
    thrd1.join();
    thrd2.join();

    return 0;
#endif

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

    // map["default\xf0\x9f\x8c\x8d"] = {};
    // map["test"] = {};

    //std::cout << typeid(decltype(map)).name() << std::endl;

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
    std::wstring wide = skate::utf_convert<std::wstring>(std::string("Narrow to wide string\xf0\x9f\x8c\x8d"));

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

    std::istringstream icsv("Header 1,-123456789,0.002\n333440,-3,44000\n\r0, -1, -2\n1,2,3\n");
    std::vector<std::string> csvline;
    std::vector<std::tuple<std::string, int, double>> tuple;

    skate::csv_options opts(',', '"', false);

    std::istringstream ijson("[\"string\",-1,1]");
    if (icsv >> skate::csv(tuple, opts))
        std::cout << "SUCCESS: " << skate::json(tuple) << '\n';
    else
        std::cout << "Failed\n";

    if (icsv >> skate::csv(tuple, opts))
        std::cout << "SUCCESS: " << skate::json(tuple) << '\n' << skate::csv(tuple, opts) << '\n';

    std::array<std::string, 10> array;

    std::cout << skate::csv(std::make_tuple(std::string("std::string"), double(3.14159265), true, -1, 0, nullptr, skate::is_trivial_tuple_base<decltype(std::make_tuple(std::string{}, 0))>::value));
    std::cout << skate::json(array);

    time_t now = time(NULL);
    tm t;
    skate::gmtime_r(now, t);
    std::cout << '\n' << skate::ctime(now) << '\n';
    std::cout << '\n' << skate::asctime(t) << '\n';

    return 0;

    try {
        skate::startup_wrapper wrapper;
        skate::socket_server<skate::poll_socket_watcher> server;

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
        }, [](Skate::Socket *connection, Skate::socket_watch_flags watching, Skate::socket_watch_flags event) -> Skate::socket_watch_flags {
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

    return 0;
}
