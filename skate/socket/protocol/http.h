#ifndef SKATE_HTTP_H
#define SKATE_HTTP_H

#include "../server.h"

namespace skate {
    namespace impl {
        struct less_case_insensitive {
            bool operator()(const std::string &lhs, const std::string &rhs) const {
                using namespace std;

                for (size_t i = 0; i < min(lhs.size(), rhs.size()); ++i) {
                    const auto l = skate::tolower(lhs[i]);
                    const auto r = skate::tolower(rhs[i]);

                    if (l != r)
                        return l < r;
                }

                return lhs.size() < rhs.size();
            }
        };
    }

    // Contains one HTTP request from client -> server
    class http_client_request {
        unsigned int m_major, m_minor;
        std::string m_method;

        skate::url m_url;
        bool m_server_request; // Whether to use the url (false), or send '*' as the url (true)

        std::map<std::string, std::string, impl::less_case_insensitive> m_headers;
        std::string m_body;

    public:
        http_client_request() : m_major(1), m_minor(1), m_method("GET"), m_server_request(false) {}

        bool valid() const noexcept { return is_wildcard_request() || m_url.valid(); }
        operator bool() const noexcept { return valid(); }

        unsigned int major() const noexcept { return m_major; }
        unsigned int minor() const noexcept { return m_minor; }
        const std::string &method() const noexcept { return m_method; }
        bool is_wildcard_request() const noexcept { return m_server_request; }
        const skate::url &url() const noexcept { return m_url; }
        const std::map<std::string, std::string, impl::less_case_insensitive> &headers() const noexcept { return m_headers; }
        std::string header(const std::string &key, std::string default_value = {}) const {
            const auto it = m_headers.find(key);
            if (it == m_headers.end())
                return default_value;

            return it->second;
        }
        bool has_header(const std::string &key) const { return m_headers.find(key) != m_headers.end(); }
        const std::string &body() const noexcept { return m_body; }
#ifdef SKATE_JSON_H
        skate::json_value json(skate::json_value default_value = {}) const {
            bool err = false;
            skate::json_value result = skate::from_json<skate::json_value>(m_body, &err);
            if (err)
                return default_value;

            return result;
        }
#endif

        http_client_request &set_major(unsigned int major) noexcept { m_major = major; return *this; }
        http_client_request &set_minor(unsigned int minor) noexcept { m_minor = minor; return *this; }
        http_client_request &set_method(const std::string &method) {
            m_method = uppercase_ascii_copy(method);

            const size_t offset = m_method.find_first_of(" \r\n\t");
            if (offset != m_method.npos)
                m_method.erase(offset);

            return *this;
        }
        http_client_request &set_wildcard_request() noexcept { m_server_request = true; return *this; }
        http_client_request &set_url(skate::url url) { m_url = std::move(url); m_server_request = false; return *this; }
        http_client_request &set_body(std::string body) {
            m_body = std::move(body);
            return set_header("Content-Length", std::to_string(m_body.size()));
        }
#ifdef SKATE_JSON_H
        http_client_request &set_body(const skate::json_value &body) {
            m_body = skate::to_json(body);
            return set_header("Content-Type", "application/json").
                   set_header("Content-Length", std::to_string(m_body.size()));
        }
#endif
        http_client_request &set_headers(const std::map<std::string, std::string> &headers) {
            m_headers.clear();

            for (const auto &header: headers)
                m_headers[header.first] = header.second;

            return *this;
        }
        http_client_request &set_header(std::string key, std::string value) { m_headers[std::move(key)] = std::move(value); return *this; }
        http_client_request &erase_header(const std::string &key) { m_headers.erase(key); return *this; }

        http_client_request &finalize() {
            set_header("Host", url().get_hostname());
            erase_header("Transfer-Encoding");

            return *this;
        }
    };

    // Contains one HTTP response from server -> client
    class http_server_response {
        unsigned int m_major, m_minor;
        unsigned int m_code;
        std::string m_status;

        std::map<std::string, std::string, impl::less_case_insensitive> m_headers;
        std::string m_body;

    public:
        http_server_response() : m_major(1), m_minor(1), m_code(0) {}

        bool valid() const noexcept { return m_code != 0; }
        operator bool() const noexcept { return valid(); }

        unsigned int major() const noexcept { return m_major; }
        unsigned int minor() const noexcept { return m_minor; }
        unsigned int code() const noexcept { return m_code; }
        const std::string &status() const noexcept { return m_status; }
        const std::map<std::string, std::string, impl::less_case_insensitive> &headers() const noexcept { return m_headers; }
        std::string header(const std::string &key, std::string default_value = {}) const {
            const auto it = m_headers.find(key);
            if (it == m_headers.end())
                return default_value;

            return it->second;
        }
        bool has_header(const std::string &key) const { return m_headers.find(key) != m_headers.end(); }
        const std::string &body() const noexcept { return m_body; }
#ifdef SKATE_JSON_H
        skate::json_value json(skate::json_value default_value = {}) const {
            bool err = false;
            skate::json_value result = skate::from_json<skate::json_value>(m_body, &err);
            if (err)
                return default_value;

            return result;
        }
#endif

        http_server_response &set_major(unsigned int major) noexcept { m_major = major; return *this; }
        http_server_response &set_minor(unsigned int minor) noexcept { m_minor = minor; return *this; }
        http_server_response &set_code(unsigned int code) noexcept { m_code = code; return *this; }
        http_server_response &set_status(std::string status) {
            m_status = std::move(status);
            m_status.erase(std::remove_if(m_status.begin(), m_status.end(), [](char c) { return c == '\n' || c == '\r'; }), m_status.end());
            return *this;
        }
        http_server_response &set_body(std::string body) {
            m_body = std::move(body);
            return set_header("Content-Length", std::to_string(m_body.size()));
        }
#ifdef SKATE_JSON_H
        http_server_response &set_body(const skate::json_value &body) {
            m_body = skate::to_json(body);
            return set_header("Content-Type", "application/json").
                   set_header("Content-Length", std::to_string(m_body.size()));
        }
#endif
        http_server_response &set_headers(const std::map<std::string, std::string> &headers) {
            m_headers.clear();

            for (const auto &header: headers)
                m_headers[header.first] = header.second;

            return *this;
        }
        http_server_response &set_header(std::string key, std::string value) { m_headers[std::move(key)] = std::move(value); return *this; }
        http_server_response &erase_header(const std::string &key) { m_headers.erase(key); return *this; }

        http_server_response &finalize() { return *this; }
    };

    class http_client_socket : public skate::tcp_socket {
        enum class status {
            reading_status,
            reading_headers,
            reading_body
        };

        const int64_t length_until_close = -1;
        const int64_t length_chunked = -2;

        status m_status;
        std::string m_response_line;
        http_server_response m_response;
        int64_t m_expected_length;                        // Expected length if specified, -1 if closed connection indicates end of body, -2 if chunked
        std::deque<http_client_request> m_requests;

        void emit_response(std::error_code &ec) {
            auto request = std::move(m_requests.front());
            m_requests.pop_front();

            const bool disconnect_after = m_response.header("Connection").find("close") != std::string::npos ||
                                          request.header("Connection").find("close") != std::string::npos;

            http_response_received(std::move(request), std::move(m_response));

            m_response = {};
            m_status = status::reading_status;

            if (disconnect_after)
                disconnect(ec);
        }

    protected:
        http_client_socket(skate::system_socket_descriptor desc, skate::socket_state current_state, bool blocking)
            : tcp_socket(desc, current_state, blocking)
            , m_status(status::reading_status)
            , m_expected_length(0)
        {}

        virtual void ready_read(std::error_code &ec) final override {
            if (m_status != status::reading_body) {
                skate::tcp_socket::read(ec, m_response_line, 1);

                if (!ends_with(m_response_line, "\r\n")) {
                    if (m_response_line.size() > 1024 * 1024)
                        ec = std::make_error_code(std::errc::bad_message);
                    return;
                }

                // Remove trailing "\r\n"
                m_response_line.pop_back();
                m_response_line.pop_back();

                switch (m_status) {
                    default:
                    case status::reading_status: {
                        if (m_response_line.empty()) { // Ignore empty lines preceding status line
                            m_response_line.clear();
                            return;
                        }

                        const char *start = m_response_line.c_str();
                        char *end = nullptr;

                        if (!starts_with(m_response_line, "HTTP/") || !isdigit(start[5])) {
                            ec = std::make_error_code(std::errc::bad_message);
                            return;
                        }
                        start += 5;

                        // Get HTTP major version
                        m_response.set_major(std::max(0L, std::min(strtol(start, &end, 10), 255L)));
                        start = end;

                        if (*start++ != '.') {
                            ec = std::make_error_code(std::errc::bad_message);
                            return;
                        }

                        // Get HTTP minor version
                        m_response.set_minor(std::max(0L, std::min(strtol(start, &end, 10), 255L)));
                        start = end;

                        while (*start == ' ')
                            ++start;

                        // Get HTTP status code
                        if (!isdigit(start[0]) ||
                            !isdigit(start[1]) ||
                            !isdigit(start[2])) {
                            ec = std::make_error_code(std::errc::bad_message);
                            return;
                        }

                        m_response.set_code(std::max(0L, std::min(strtol(start, &end, 10), long(INT_MAX))));
                        start = end;

                        while (*start == ' ')
                            ++start;

                        // Get HTTP status reason
                        m_response.set_status(m_response_line.substr(start - m_response_line.c_str()));

                        m_response_line.clear();
                        m_status = status::reading_headers;

                        break;
                    }
                    case status::reading_headers: {
                        if (m_response_line.empty()) { // Done reading headers if blank line
                            // Check if no body expected at all
                            if (m_response.code() / 100 == 1) {
                                // TODO: handle 100-continue if needed
                                m_status = status::reading_status;
                                return;
                            } else if (m_response.code() == 204 || m_response.code() == 304 || m_requests.front().method() == "HEAD") {
                                emit_response(ec);
                                return;
                            }

                            if (0) {
                                // TODO: Check Transfer-Encoding to see if it's chunked
                            } else if (m_response.has_header("Content-Length")) {
                                // Check Content-Length to see length of response
                                errno = 0;
                                m_expected_length = std::max(0LL, strtoll(m_response.header("Content-Length").c_str(), nullptr, 10));
                                if (errno == ERANGE) {
                                    ec = std::make_error_code(std::errc::bad_message);
                                    return;
                                }
                            } else if (0) {
                                // TODO: Check multipart/byteranges to see if the transfer length can be deduced
                            } else {
                                m_expected_length = length_until_close;
                            }

                            m_status = status::reading_body;

                            return;
                        }

                        if (isspace_or_tab(m_response_line[0])) {
                            // TODO: handle folded headers
                            return;
                        }

                        // Find end of header key
                        const size_t colon = m_response_line.find(':');
                        if (colon == m_response_line.npos) {
                            ec = std::make_error_code(std::errc::bad_message);
                            return;
                        }

                        size_t value_start = colon + 1;

                        // Skip leading whitespace before value
                        while (value_start < m_response_line.size() && isspace_or_tab(m_response_line[value_start]))
                            ++value_start;

                        m_response.set_header(m_response_line.substr(0, colon), m_response_line.substr(value_start));
                        m_response_line.clear();

                        break;
                    }
                }
            } else { // Reading body (m_response_line is repurposed to read body)
                if (m_expected_length == length_until_close) {
                    skate::tcp_socket::read_all(ec, m_response_line);
                } else if (m_expected_length == length_chunked) {
                    // TODO: chunked transfer reading
                } else {
                    m_expected_length -= skate::tcp_socket::read(ec, m_response_line, std::min<uintmax_t>(m_expected_length, SIZE_MAX));

                    if (m_expected_length == 0) {
                        m_response.set_body(std::move(m_response_line));
                        m_response_line = {};

                        emit_response(ec);
                    }
                }
            }
        }

        virtual void disconnected(std::error_code &ec) override {
            if (m_status == status::reading_body) {
                if (m_expected_length == length_until_close) {
                    m_response.set_body(std::move(m_response_line));
                    m_response_line = {};

                    emit_response(ec);
                } else {
                    ec = std::make_error_code(std::errc::bad_message);
                }
            }
        }

        virtual void http_response_received(http_client_request &&request, http_server_response &&response) {
            std::cout << response.code() << ' ' << response.status() << '\n';

            for (const auto &header: response.headers())
                std::cout << header.first << ": " << header.second << '\n';
            std::cout << '\n';

            std::cout << response.body() << '\n';
        }

        virtual void error(std::error_code ec) override {
            std::cout << "HTTP error happened: " << ec.message() << '\n';
        }

    public:
        http_client_socket() {}
        virtual ~http_client_socket() {}

        static http_server_response http_write_request_sync(std::error_code &ec, http_client_request request) {
            http_client_socket http;

            auto resolved = http.resolve(ec, network_address(request.url().get_hostname()));
            http.connect_sync(ec, resolved);
            http.http_write_request(ec, std::move(request));

            // TODO: extract result synchronously

            return {};
        }

        void http_write_request(std::error_code &ec, http_client_request request) {
            if (ec)
                return;

            request.finalize();
            m_requests.push_back(request);

            if (request.method().empty()) {
                ec = std::make_error_code(std::errc::bad_message);
                return;
            }

            std::string txbuf;
            std::string path = request.is_wildcard_request()? "*": request.url().get_path_and_query_and_fragment();
            if (path.empty())
                path = '/';

            // Generate request line
            txbuf += request.method();
            txbuf += ' ';
            txbuf += path;
            txbuf += " HTTP/";
            txbuf += std::to_string(request.major());
            txbuf += '.';
            txbuf += std::to_string(request.minor());
            txbuf += "\r\n";

            // Generate headers
            for (const auto &header: request.headers()) {
                txbuf += header.first;
                txbuf += ": ";
                txbuf += header.second;
                txbuf += "\r\n";
            }
            txbuf += "\r\n";

            // Write status and headers
            write(ec, txbuf);

            std::cout << txbuf << std::endl;

            // Write body (if any)
            write(ec, request.body());
        }
    };

    // TODO: doesn't support Expect: 100-continue at all
    class http_server_socket : public skate::tcp_socket {
        enum class status {
            reading_status,
            reading_headers,
            reading_body
        };

        const int64_t length_chunked = -1;

        status m_status;
        std::string m_request_line;
        http_client_request m_request;
        int64_t m_expected_length;                        // Expected length of request if specified, -1 if chunked

        void http_write_response(std::error_code &ec, http_server_response response) {
            response.finalize();

            std::string txbuf;

            // Generate response status line
            txbuf += "HTTP/";
            txbuf += std::to_string(response.major());
            txbuf += '.';
            txbuf += std::to_string(response.minor());
            txbuf += ' ';
            txbuf += std::to_string(response.code());
            txbuf += ' ';
            txbuf += response.status();
            txbuf += "\r\n";

            // Generate headers
            for (const auto &header: response.headers()) {
                txbuf += header.first;
                txbuf += ": ";
                txbuf += header.second;
                txbuf += "\r\n";
            }
            txbuf += "\r\n";

            // Write status and headers
            write(ec, txbuf);

            // Write body (if any)
            write(ec, response.body());
        }

    protected:
        http_server_socket(skate::system_socket_descriptor desc, skate::socket_state current_state, bool blocking)
            : tcp_socket(desc, current_state, blocking)
            , m_status(status::reading_status)
            , m_expected_length(0)
        {}

        virtual void ready_read(std::error_code &ec) final override {
            if (m_status != status::reading_body) {
                skate::tcp_socket::read(ec, m_request_line, 1);

                if (!ends_with(m_request_line, "\r\n")) {
                    if (m_request_line.size() > 1024 * 1024)
                        ec = std::make_error_code(std::errc::bad_message);
                    return;
                }

                // Remove trailing "\r\n"
                m_request_line.pop_back();
                m_request_line.pop_back();

                switch (m_status) {
                    default:
                    case status::reading_status: {
                        if (m_request_line.empty()) { // Ignore empty lines preceding status line
                            m_request_line.clear();
                            return;
                        }

                        std::cout << m_request_line << std::endl;

                        const char *start = m_request_line.c_str();
                        char *end = nullptr;

                        // Get method
                        size_t offset = m_request_line.find(' ');
                        if (offset == std::string::npos) {
                            ec = std::make_error_code(std::errc::bad_message);
                            return;
                        }

                        m_request.set_method(m_request_line.substr(0, offset));
                        start = m_request_line.c_str() + offset;
                        while (*start == ' ')
                            ++start;

                        // Get URL
                        offset = m_request_line.find(' ', start - m_request_line.c_str());
                        if (offset == std::string::npos) {
                            ec = std::make_error_code(std::errc::bad_message);
                            return;
                        }

                        const std::string url = m_request_line.substr(start - m_request_line.c_str(), offset - (start - m_request_line.c_str()));
                        start = m_request_line.c_str() + offset;
                        while (*start == ' ')
                            ++start;

                        // Get HTTP version
                        if (!starts_with(start, "HTTP/") || !isdigit(start[5])) {
                            ec = std::make_error_code(std::errc::bad_message);
                            return;
                        }
                        start += 5;

                        // Get HTTP major version
                        m_request.set_major(std::max(0L, std::min(strtol(start, &end, 10), 255L)));
                        start = end;

                        if (*start++ != '.') {
                            ec = std::make_error_code(std::errc::bad_message);
                            return;
                        }

                        // Get HTTP minor version
                        m_request.set_minor(std::max(0L, std::min(strtol(start, &end, 10), 255L)));

                        m_request_line.clear();
                        m_status = status::reading_headers;

                        break;
                    }
                    case status::reading_headers: {
                        if (m_request_line.empty()) { // Done reading headers if blank line
                            if (0) {
                                // TODO: Check Transfer-Encoding to see if it's chunked
                            } else if (m_request.has_header("Content-Length")) {
                                // Check Content-Length to see length of response
                                errno = 0;
                                m_expected_length = std::max(0LL, strtoll(m_request.header("Content-Length").c_str(), nullptr, 10));
                                if (errno == ERANGE) {
                                    ec = std::make_error_code(std::errc::bad_message);
                                    return;
                                }
                            } else if (0) {
                                // TODO: Check multipart/byteranges to see if the transfer length can be deduced
                            } else {
                                m_status = status::reading_status;

                                http_write_response(ec, http_request_received(std::move(m_request)));

                                return;
                            }

                            m_status = status::reading_body;

                            return;
                        }

                        if (isspace_or_tab(m_request_line[0])) {
                            // TODO: handle folded headers
                            return;
                        }

                        // Find end of header key
                        const size_t colon = m_request_line.find(':');
                        if (colon == m_request_line.npos) {
                            ec = std::make_error_code(std::errc::bad_message);
                            return;
                        }

                        size_t value_start = colon + 1;

                        // Skip leading whitespace before value
                        while (value_start < m_request_line.size() && isspace_or_tab(m_request_line[value_start]))
                            ++value_start;

                        m_request.set_header(m_request_line.substr(0, colon), m_request_line.substr(value_start));
                        m_request_line.clear();

                        break;
                    }
                }
            } else { // Reading body (m_response_line is repurposed to read body)
                if (m_expected_length == length_chunked) {
                    // TODO: chunked transfer reading
                } else {
                    m_expected_length -= skate::tcp_socket::read(ec, m_request_line, std::min<uintmax_t>(m_expected_length, SIZE_MAX));

                    if (m_expected_length == 0) {
                        m_request.set_body(std::move(m_request_line));
                        m_request_line = {};

                        http_write_response(ec, http_request_received(std::move(m_request)));
                    }
                }
            }
        }

        virtual http_server_response http_request_received(http_client_request &&request) {
            std::cout << "Request received\n";

            std::cout << request.method() << ' ' << request.url().to_string() << " HTTP/" << request.major() << '.' << request.minor() << std::endl;
            for (const auto &header: request.headers()) {
                std::cout << header.first << ": " << header.second << std::endl;
            }
            std::cout << std::endl;
            std::cout << request.body() << std::endl;

            return {};
        }

        virtual std::unique_ptr<socket> create(system_socket_descriptor desc, socket_state current_state, bool is_blocking) override {
            return std::unique_ptr<socket>{new http_server_socket(desc, current_state, is_blocking)};
        }

    public:
        http_server_socket() {}
        virtual ~http_server_socket() {}
    };
}

#endif // SKATE_HTTP_H
