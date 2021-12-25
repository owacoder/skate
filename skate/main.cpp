#define NOMINMAX
//#include <afx.h>

#include <iostream>
#include "threadbuffer.h"
#include <thread>
#include <vector>

#include "io/buffer.h"
#include "containers/tree.h"
#include "containers/sparse_array.h"

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

#include "containers/abstract_list.h"
#include "containers/abstract_map.h"
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
    skate::sparse_array<uint8_t> sparse;
    std::vector<uint8_t> dense;

    skate::benchmark([&]() {
        for (size_t i = 0; i < 1000000090; ++i)
            sparse.push_back(rand());
    }, "Building sparse");

    skate::benchmark([&]() {
        for (size_t i = 0; i < 1000000090; ++i)
            dense.push_back(rand());
    }, "Building dense");

#if 0
    for (size_t r = 0; r < sparse.runs(); ++r) {
        std::cout << "Run " << (r+1) << ":\n";
        for (size_t k = sparse.run_begin(r); k < sparse.run_end(r); ++k) {
            std::cout << "  " << k << ": " << sparse.at(k) << '\n';
        }
    }
#endif

    skate::benchmark([&]() {
        sparse.erase(0, sparse.size());
    }, "Erasing sparse");

    skate::benchmark([&]() {
        dense.erase(dense.begin(), dense.begin() + std::min<size_t>(1000000000, dense.size()));
    }, "Erasing dense");

    size_t calc_stored = 0;
    for (size_t r = 0; r < sparse.runs(); ++r) {
        std::cout << "Run " << (r+1) << ":\n";
        for (size_t k = sparse.run_begin(r); k < sparse.run_end(r); ++k) {
            std::cout << "  " << k << ": " << int(sparse.at(k)) << '\n';
            ++calc_stored;
        }
    }

    std::cout << sparse.stored() << ',' << calc_stored << '\n';

    return;

    std::initializer_list<int> il = { 0, 1, 2, 3, 4, 5 };

    std::vector<int> v = il;
    std::list<int> l = il;
    std::forward_list<int> fl = il;
    std::map<int, int> m = {{0, 5}, {1, 4}, {2, 3}, {3, 2}, {4, 1}, {5, 0}};
    // CArray<int, int> ca;
    // CList<int, int> cl;
    // CString s;

    std::cout << "map: " << skate::json(skate::abstract::keys(m)) << '\n';
    std::cout << "map: " << skate::json(skate::abstract::values(m)) << '\n';

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
    skate::url url("http://username:password@www.jw.org/../path/content=5/../?#%20query=%65bc");

    url.set_path("/path/content");

    std::cout << url.to_string(skate::url::encoding::raw) << '\n';
    std::cout << url.valid() << '\n';
    std::cout << "scheme: " << url.get_scheme() << '\n';
    std::cout << "username: " << url.get_username(skate::url::encoding::percent) << '\n';
    std::cout << "password: " << url.get_password(skate::url::encoding::percent) << '\n';
    std::cout << "host: " << url.get_host() << '\n';
    std::cout << "port: " << url.get_port() << '\n';
    std::cout << "path: " << url.get_path(skate::url::encoding::percent) << '\n';
    std::cout << "query: " << url.get_query(skate::url::encoding::percent) << '\n';
    std::cout << "fragment: " << url.get_fragment(skate::url::encoding::percent) << '\n';

    skate::startup_wrapper wrapper;
    skate::socket_server<> server;
    skate::http_client_socket http;
    skate::http_server_socket httpserve;
    std::error_code ec;

    skate::http_client_request req;
    req.set_url("http://territory.ddns.net");
    req.set_header("Connection", "close");

    auto resolved = http.resolve(ec, { req.url().get_host(), req.url().get_port(80) });
    http.set_blocking(ec, false);
    if (resolved.size())
        http.connect_sync(ec, resolved);

    httpserve.bind(ec, skate::socket_address("192.168.1.100", 80));
    httpserve.listen(ec);

    auto save = ec;
    std::cout << skate::json(resolved) << save.message() << std::endl;

    // http.listen(ec);
    std::cout << save.message() << " " << ec.message() << std::endl;
    server.serve_socket(&http);
    server.serve_socket(&httpserve);
    std::cout << "server running" << std::endl;

    http.http_write_request(ec, req);
    server.run();
}

void server_test() {
    skate::startup_wrapper wrapper;
    skate::socket_server<> server;
    skate::http_server_socket httpserve;

    std::error_code ec;

    httpserve.bind(ec, skate::socket_address("192.168.1.100", 80));
    httpserve.listen(ec);

    std::cout << ec.message() << "\n";

    server.serve_socket(&httpserve);
    server.run();
}

struct Point {
    int x, y;
    Point() : x(), y() {}
};

template<typename StreamChar>
void skate_json(std::basic_istream<StreamChar> &, Point &) {

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

namespace skate {
    template<typename Reader, typename Writer>
    void copy(Reader &reader, Writer &writer) {
        while (reader.valid()) {
            writer.push_back(reader.get());
            reader.next();
        }
    }

    template<typename Iterator>
    class iterator_writer {
        Iterator m_out;

    public:
        constexpr iterator_writer(Iterator it) : m_out(it) {}

        template<typename T>
        iterator_writer &push_back(T &&v) { *m_out++ = std::forward<T>(v); return *this; }

        constexpr void start() noexcept {}
        constexpr void finish() noexcept {}
        constexpr bool failed() const noexcept { return false; }
    };

    template<typename Range, typename Iterator = decltype(begin(std::declval<Range>()))>
    class iterator_reader {
        Iterator m_begin, m_end;

    public:
        constexpr iterator_reader(const Range &range) : m_begin(begin(range)), m_end(end(range)) {}

        constexpr bool valid() const noexcept { return m_begin != m_end; }
        constexpr decltype(*m_begin) get() const { return *m_begin; }
        constexpr iterator_reader &next() { return ++m_begin, *this; }

        constexpr bool failed() const noexcept { return false; }
    };

    // Writes little-endian encoding of integers. Consumes unsigned integers, outputs uint8_t
    template<typename Writer>
    class le_encode {
        Writer &m_writer;

    public:
        constexpr le_encode(Writer &writer) : m_writer(writer) {}

        template<typename T>
        le_encode &push_back(T &&value) {
            static_assert(std::is_unsigned<T>::value, "Only unsigned integer types can be serialized");

            for (size_t i = 0; i < std::numeric_limits<T>::digits; i += 8)
                m_writer.push_back(uint8_t(value >> i));

            return *this;
        }

        constexpr void start() { m_writer.start(); }
        constexpr void finish() { m_writer.finish(); }
        constexpr bool failed() const { return m_writer.failed(); }
    };

    template<typename T, typename Reader>
    class le_decode {
        static_assert(std::is_unsigned<T>::value, "Only unsigned integer types can be parsed");

        Reader &m_reader;
        T m_state;
        bool m_state_valid;
        bool m_failed;

    public:
        constexpr le_decode(Reader &reader, T) : m_reader(reader), m_state(0), m_state_valid(false), m_failed(false) {}

        bool valid() {
            if (m_failed)
                return true;
            else if (!m_state_valid) {
                for (size_t i = 0; i < std::numeric_limits<T>::digits; i += 8, m_reader.next()) {
                    if (!m_reader.valid()) {
                        m_failed = i > 0;
                        return false;
                    }

                    m_state |= T(uint8_t(m_reader.get())) << i;
                }

                m_state_valid = true;
            }

            return true;
        }
        const T &get() const noexcept { return m_state; }
        le_decode &next() { return m_state_valid = false, *this; }

        constexpr bool failed() const { return m_failed || m_reader.failed(); }
    };

    // Writes little-endian encoding of integers. Consumes unsigned integers, outputs uint8_t
    template<typename Writer>
    class be_encode {
        Writer &m_writer;

    public:
        constexpr be_encode(Writer &writer) : m_writer(writer) {}

        template<typename T>
        be_encode &push_back(T value) {
            static_assert(std::is_unsigned<T>::value, "Only unsigned integer types can be serialized");

            for (size_t i = std::numeric_limits<T>::digits; i > 0; )
                m_writer.push_back(uint8_t(value >> (i -= 8)));

            return *this;
        }

        constexpr void start() { m_writer.start(); }
        constexpr void finish() { m_writer.finish(); }
        constexpr bool failed() const { return m_writer.failed(); }
    };

    // Writes little-endian encoding of integers. Consumes unsigned integers, outputs uint8_t
    template<typename T, typename Reader>
    class be_decode {
        static_assert(std::is_unsigned<T>::value, "Only unsigned integer types can be parsed");

        Reader &m_reader;
        T m_state;
        bool m_state_valid;
        bool m_failed;

    public:
        constexpr be_decode(Reader &reader, T) : m_reader(reader), m_state(0), m_state_valid(false), m_failed(false) {}

        bool valid() {
            if (m_failed)
                return true;
            else if (!m_state_valid) {
                for (size_t i = 0; i < std::numeric_limits<T>::digits; i += 8, m_reader.next()) {
                    if (!m_reader.valid()) {
                        m_failed = i > 0;
                        return false;
                    }

                    m_state = (m_state << 8) | T(uint8_t(m_reader.get()));
                }

                m_state_valid = true;
            }

            return true;
        }
        const T &get() const noexcept { return m_state; }
        be_decode &next() { return m_state_valid = false, *this; }

        constexpr bool failed() const { return m_failed || m_reader.failed(); }
    };

    class unicode {
        uint32_t cp;

        // Helper function that calculates the surrogate pair for a codepoint, given the codepoint - 0x10000 (see utf16_surrogates())
        static constexpr std::pair<uint16_t, uint16_t> utf16_surrogate_helper(uint32_t subtracted_codepoint) noexcept {
            return { 0xd800u | (subtracted_codepoint >> 10), 0xdc00u | (subtracted_codepoint & 0x3ff) };
        }

    public:
        static constexpr uint32_t utf_max = 0x10fffful;
        static constexpr uint32_t utf_mask = 0x1ffffful;
        static constexpr unsigned utf_max_bytes = 5;
        static constexpr uint32_t utf_error = 0x8000fffdul;

        constexpr unicode(uint32_t codepoint = 0) noexcept : cp(codepoint <= utf_max ? codepoint : utf_error) {}
        template<typename T>
        constexpr unicode(T hi_surrogate, T lo_surrogate) noexcept
            : cp((hi_surrogate >= 0xd800u && hi_surrogate <= 0xdbffu) &&
                 (lo_surrogate >= 0xdc00u && lo_surrogate <= 0xdfffu) ? (((hi_surrogate & 0x3fful) << 10) | (lo_surrogate & 0x3fful)) + 0x10000ul : utf_error)
        {}

        constexpr bool is_utf16_surrogate() const noexcept { return cp >= 0xd800u && cp <= 0xdfffu; }
        constexpr bool is_valid() const noexcept { return cp <= utf_max; }

        constexpr uint32_t value() const noexcept { return cp & utf_mask; }

        // Returns number of bytes needed for UTF-8 encoding
        constexpr unsigned int utf8_size() const noexcept {
            return cp <= 0x7fu? 1:
                   cp <= 0xffu? 2:
                   cp <= 0xffffu? 3:
                   cp <= utf_max? 4: 0;
        }

        // Returns number of codepoints (either 1 or 2) needed for UTF-16 encoding
        constexpr unsigned int utf16_size() const noexcept {
            return cp <= 0xffffu ? 1 : 2;
        }

        // Returns pair of surrogates for the given codepoint (utf16_size() == 2), or if no surrogates are needed (utf16_size() == 1), equal codepoints with the actual value of the codepoint
        // This allows bypassing using utf16_size, just calling this function, and then testing the returned pair for equality
        constexpr std::pair<uint16_t, uint16_t> utf16_surrogates() const noexcept {
            return cp <= 0xffffu ? std::pair<uint16_t, uint16_t>{ cp, cp } : utf16_surrogate_helper(cp - 0x10000);
        }

        constexpr bool operator==(unicode other) const noexcept { return cp == other.cp; }
        constexpr bool operator!=(unicode other) const noexcept { return cp != other.cp; }
        constexpr bool operator <(unicode other) const noexcept { return cp  < other.cp; }
        constexpr bool operator >(unicode other) const noexcept { return cp  > other.cp; }
        constexpr bool operator<=(unicode other) const noexcept { return cp <= other.cp; }
        constexpr bool operator>=(unicode other) const noexcept { return cp >= other.cp; }
    };

    // Writes UTF-8 encoding of unicode codepoints. Consumes objects of type unicode, outputs uint8_t
    template<typename Writer>
    class utf8encode {
        Writer &m_writer;
        bool m_failed;

    public:
        constexpr utf8encode(Writer &writer) : m_writer(writer), m_failed(false) {}

        utf8encode &push_back(unicode value) {
            if (value.is_utf16_surrogate()) {
                m_failed = true;
                return *this;
            }

            switch (value.utf8_size()) {
                default:
                    m_failed = true;
                    break;
                case 1:
                    m_writer.push_back(uint8_t(value.value()));
                    break;
                case 2:
                    m_writer.push_back(uint8_t(0xC0 | (value.value() >> 6)));
                    m_writer.push_back(uint8_t(0x80 | (value.value() & 0x3f)));
                    break;
                case 3:
                    m_writer.push_back(uint8_t(0xE0 | ((value.value() >> 12))));
                    m_writer.push_back(uint8_t(0x80 | ((value.value() >>  6) & 0x3f)));
                    m_writer.push_back(uint8_t(0x80 | ((value.value()      ) & 0x3f)));
                    break;
                case 4:
                    m_writer.push_back(uint8_t(0xF0 | ((value.value() >> 18))));
                    m_writer.push_back(uint8_t(0x80 | ((value.value() >> 12) & 0x3f)));
                    m_writer.push_back(uint8_t(0x80 | ((value.value() >>  6) & 0x3f)));
                    m_writer.push_back(uint8_t(0x80 | ((value.value()      ) & 0x3f)));
                    break;
            }

            return *this;
        }

        constexpr void start() { m_writer.start(); }
        constexpr void finish() { m_writer.finish(); }
        constexpr bool failed() const { return m_failed || m_writer.failed(); }
    };

    // Writes UTF-16 encoding of unicode codepoints. Consumes objects of type unicode, outputs uint16_t
    template<typename Writer>
    class utf16encode {
        Writer &m_writer;
        bool m_failed;

    public:
        constexpr utf16encode(Writer &writer) : m_writer(writer), m_failed(false) {}

        utf16encode &push_back(unicode value) {
            if (value.is_utf16_surrogate()) {
                m_failed = true;
                return *this;
            }

            const auto surrogates = value.utf16_surrogates();

            m_writer.push_back(surrogates.first);

            if (surrogates.first != surrogates.second)
                m_writer.push_back(surrogates.second);

            return *this;
        }

        constexpr void start() { m_writer.start(); }
        constexpr void finish() { m_writer.finish(); }
        constexpr bool failed() const { return m_failed || m_writer.failed(); }
    };

    inline constexpr char nibble_to_hex(uint8_t nibble) noexcept {
        return "0123456789ABCDEF"[nibble & 0xf];
    }

    inline constexpr char nibble_to_hex_lower(uint8_t nibble) noexcept {
        return "0123456789abcdef"[nibble & 0xf];
    }

    inline constexpr char nibble_to_hex(uint8_t nibble, bool uppercase) noexcept {
        return uppercase ? nibble_to_hex(nibble) : nibble_to_hex_lower(nibble);
    }

    inline constexpr int hex_to_nibble(int c) noexcept {
        return c >= '0' && c <= '9' ? c - '0' :
               c >= 'A' && c <= 'F' ? c - 'A' + 10 :
               c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                                      -1;
    }

    template<typename Writer>
    class hexencode {
        Writer &m_writer;

    public:
        constexpr hexencode(Writer &writer) : m_writer(writer) {}

        hexencode &push_back(std::uint8_t value) {
            m_writer.push_back(nibble_to_hex(value >> 4));
            m_writer.push_back(nibble_to_hex(value & 0xf));
            return *this;
        }

        constexpr void start() { m_writer.start(); }
        constexpr void finish() { m_writer.finish(); }
        constexpr bool failed() const { return m_writer.failed(); }
    };

    template<typename Writer>
    class hexencode_lower {
        Writer &m_writer;

    public:
        constexpr hexencode_lower(Writer &writer) : m_writer(writer) {}

        hexencode_lower &push_back(std::uint8_t value) {
            m_writer.push_back(nibble_to_hex_lower(value >> 4));
            m_writer.push_back(nibble_to_hex_lower(value & 0xf));
            return *this;
        }

        constexpr void start() { m_writer.start(); }
        constexpr void finish() { m_writer.finish(); }
        constexpr bool failed() const { return m_writer.failed(); }
    };

    struct base64options {
        base64options(const char alpha[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
            : padding('=')
        {
            std::copy_n(alpha, alphabet.size(), alphabet.begin());
        }

        std::array<char, 64> alphabet;
        char padding;
    };

    template<typename Writer>
    class base64encode {
        Writer &m_writer;
        unsigned long m_state;
        unsigned m_bytes_in_state;

        base64options m_options;

    public:
        base64encode(Writer &writer, base64options options = {}) : m_writer(writer), m_state(0), m_bytes_in_state(0), m_options(options) {}
        ~base64encode() { finish(); }

        void push_back(uint8_t value) {
            m_state = (m_state << 8) | value;
            ++m_bytes_in_state;

            if (m_bytes_in_state == 3) {
                m_writer.push_back(m_options.alphabet[(m_state >> 18)       ]);
                m_writer.push_back(m_options.alphabet[(m_state >> 12) & 0x3f]);
                m_writer.push_back(m_options.alphabet[(m_state >>  6) & 0x3f]);
                m_writer.push_back(m_options.alphabet[(m_state      ) & 0x3f]);

                m_state = 0;
                m_bytes_in_state = 0;
            }
        }

        constexpr void start() { m_writer.start(); }

        void finish() {
            if (m_bytes_in_state) {
                m_writer.push_back(m_options.alphabet[(m_state >> 18)       ]);
                m_writer.push_back(m_options.alphabet[(m_state >> 12) & 0x3f]);

                if (m_bytes_in_state == 2) {
                    m_writer.push_back(m_options.alphabet[(m_state >>  6) & 0x3f]);

                    if (m_options.padding)
                        m_writer.push_back(m_options.padding);
                } else if (m_options.padding) {
                    m_writer.push_back(m_options.padding);
                    m_writer.push_back(m_options.padding);
                }
            }

            m_state = 0;
            m_bytes_in_state = 0;

            m_writer.finish();
        }

        constexpr bool failed() const { return m_writer.failed(); }
    };

    template<typename Writer>
    class cstyle_escape {
        Writer &m_writer;

    public:
        constexpr cstyle_escape(Writer &writer) : m_writer(writer) {}

        cstyle_escape &push_back(uint8_t value) {
            switch (value) {
                case 0x07: m_writer.push_back('\\'); m_writer.push_back('a'); break;
                case 0x08: m_writer.push_back('\\'); m_writer.push_back('b'); break;
                case 0x09: m_writer.push_back('\\'); m_writer.push_back('t'); break;
                case 0x0A: m_writer.push_back('\\'); m_writer.push_back('n'); break;
                case 0x0B: m_writer.push_back('\\'); m_writer.push_back('v'); break;
                case 0x0C: m_writer.push_back('\\'); m_writer.push_back('f'); break;
                case 0x0D: m_writer.push_back('\\'); m_writer.push_back('r'); break;
                case '\\': m_writer.push_back('\\'); m_writer.push_back('\\'); break;
                case '\"': m_writer.push_back('\\'); m_writer.push_back('\"'); break;
                default:
                    if (value < 32 || value >= 127) {
                        m_writer.push_back('\\');
                        m_writer.push_back('0' + ((value >> 6)      ));
                        m_writer.push_back('0' + ((value >> 3) & 0x7));
                        m_writer.push_back('0' + ((value     ) & 0x7));
                    } else {
                        m_writer.push_back(value);
                    }

                    break;
            }

            return *this;
        }

        constexpr void start() { m_writer.start(); }
        constexpr void finish() { m_writer.finish(); }
        constexpr bool failed() const { return m_writer.failed(); }
    };

    template<typename Writer>
    class json_escape {
        Writer &m_writer;
        bool m_failed;

    public:
        constexpr json_escape(Writer &writer) : m_writer(writer), m_failed(false) {}

        json_escape &push_back(unicode value) {
            if (!value.is_valid()) {
                m_failed = true;
                return *this;
            }

            switch (value.value()) {
                case 0x08: m_writer.push_back('\\'); m_writer.push_back('b'); break;
                case 0x09: m_writer.push_back('\\'); m_writer.push_back('t'); break;
                case 0x0A: m_writer.push_back('\\'); m_writer.push_back('n'); break;
                case 0x0C: m_writer.push_back('\\'); m_writer.push_back('f'); break;
                case 0x0D: m_writer.push_back('\\'); m_writer.push_back('r'); break;
                case '\\': m_writer.push_back('\\'); m_writer.push_back('\\'); break;
                case '\"': m_writer.push_back('\\'); m_writer.push_back('\"'); break;
                default:
                    if (value.value() < 32 || value.value() >= 127) {
                        const auto surrogates = value.utf16_surrogates();

                        {
                            m_writer.push_back('\\');
                            m_writer.push_back('u');

                            auto hex = hexencode(m_writer);
                            hex.push_back(surrogates.first >> 8);
                            hex.push_back(surrogates.first & 0xff);
                        }

                        if (surrogates.first != surrogates.second) {
                            m_writer.push_back('\\');
                            m_writer.push_back('u');

                            auto hex = hexencode(m_writer);
                            hex.push_back(surrogates.second >> 8);
                            hex.push_back(surrogates.second & 0xff);
                        }
                    } else {
                        m_writer.push_back(uint8_t(value.value()));
                    }

                    break;
            }

            return *this;
        }

        constexpr void start() { m_writer.start(); }
        constexpr void finish() { m_writer.finish(); }
        constexpr bool failed() const { return m_failed || m_writer.failed(); }
    };
}

int main()
{
    std::ostreambuf_iterator cout(std::cout.rdbuf());
    auto out = skate::iterator_writer(cout);
    auto hex = skate::hexencode(out);
    auto be = skate::be_encode(hex);

    std::vector<uint8_t> input = { 0x80, 0x00, 0x80, 0xff };

    auto in_it = skate::iterator_reader(input);
    auto le = skate::le_decode(in_it, uint16_t());

    for (; le.valid(); le.next()) {
        be.push_back(le.get());
        std::cout << std::endl;
    }
    std::cout << le.failed() << std::endl;

    auto it = skate::iterator_writer(cout);
    auto js = skate::json_escape(it);

    js.push_back(0x1F602);

    std::cout << '\n';

    return 0;

    network_test();
    return 0;

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

#if 1
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

    std::error_code ec;
    std::cout << "Interfaces: " << skate::json(skate::socket_address::interfaces(ec), skate::json_write_options(2)) << std::endl;

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

    std::cout << skate::csv(std::make_tuple(std::string("std::string"), double(3.14159265), true, -1, 0, nullptr, skate::is_trivial_tuple<decltype(std::make_tuple(std::string{}, 0))>::value));
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
