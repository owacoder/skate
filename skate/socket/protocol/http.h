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

    // Contains one HTTP response header set (not including body, which is signalled separately)
    struct http_response {
        http_response() : major(1), minor(1), code(200) {}

        bool is_null() const { return headers.empty(); }

        unsigned char major, minor;
        short code;
        std::string status;

        skate::url url;
        std::map<std::string, std::string, impl::less_case_insensitive> headers;
    };

    // Contains one HTTP request header set (including body)
    struct http_request {
        http_request() : method("GET"), major(1), minor(1), bodystream(nullptr) {}

        std::string method;
        skate::url url;
        unsigned char major, minor;

        std::map<std::string, std::string, impl::less_case_insensitive> headers;

        // Only one of the following is used: bodystream if non-null, body otherwise
        std::streambuf *bodystream;
        std::string body;
    };

    class http_client_socket : public skate::tcp_socket {
        std::string response_buffer;

        uint64_t response_body_consumed; // If chunked, number of bytes consumed in the chunk, otherwise, total number of bytes consumed in the response
        uint64_t response_body_expected; // If chunked, number of bytes in the current chunk, otherwise, total number of bytes expected in the response
        bool response_chunked; // Whether chunked (1), or specified size (0)

        http_response current_response;
        http_request current_request;

        // Reads the headers from the response_buffer string
        http_response read_headers(std::error_code &ec) {
            if (ec)
                return {};

            // At this point, all headers are read and waiting for parsing
            // Parse the headers now

            response_body_expected = response_body_consumed = 0;
            response_chunked = false;

            http_response response;
            const char *status = response_buffer.c_str();

            {
                char *end = nullptr;

                if (memcmp(status, "HTTP/", 5) || !isdigit(status[5])) {
                    ec = std::make_error_code(std::errc::bad_message);
                    return {};
                }
                status += 5;

                // Get HTTP major version
                response.major = static_cast<unsigned char>(std::max(0L, std::min(strtol(status, &end, 10), 255L)));
                status = end;

                if (*status++ != '.') {
                    ec = std::make_error_code(std::errc::bad_message);
                    return {};
                }

                // Get HTTP minor version
                response.minor = static_cast<unsigned char>(std::max(0L, std::min(strtol(status, &end, 10), 255L)));
                status = end;

                while (*status == ' ')
                    ++status;

                // Get HTTP status code
                if (!isdigit(status[0]) ||
                        !isdigit(status[1]) ||
                        !isdigit(status[2])) {
                    ec = std::make_error_code(std::errc::bad_message);
                    return {};
                }

                response.code = static_cast<short>(strtol(status, &end, 10));
                status = end;

                while (*status == ' ')
                    ++status;

                // Get HTTP status reason
                const size_t start_offset = status - response_buffer.c_str();
                const size_t end_of_line = response_buffer.find("\r\n", start_offset);
                response.status = std::string(status, end_of_line - start_offset);
                status = response_buffer.c_str() + end_of_line + 2;
            }

            while (1) {
                const size_t start_offset = status - response_buffer.c_str();
                const size_t end_of_line = response_buffer.find("\r\n", start_offset);
                size_t colon = response_buffer.find(':', start_offset);

                if (end_of_line == start_offset) // Empty line signifies end of headers
                    break;

                if (colon == std::string::npos) {
                    ec = std::make_error_code(std::errc::bad_message);
                    return {};
                }

                std::string key{status, colon - start_offset};

                ++colon;
                while (colon < response_buffer.size() && response_buffer[colon] == ' ')
                    ++colon;

                std::string value{response_buffer.c_str() + colon, end_of_line - colon};

                response.headers[std::move(key)] = std::move(value);

                status = response_buffer.c_str() + end_of_line + 2;
            }

            http_response_received(response);

            if (response.code == 204 || response.code == 304 || response.code / 100 == 1 || current_request.method == "HEAD")
                response_body_expected = 0;
            else
                response_body_expected = strtoull(response.headers["Content-Length"].c_str(), nullptr, 10);
            response_chunked = response.headers["Transfer-Encoding"].find("chunked") != std::string::npos;

            return response;
        }

    protected:
        http_client_socket(skate::system_socket_descriptor desc, skate::socket_state current_state, bool blocking)
            : tcp_socket(desc, current_state, blocking)
        {}

        // Data is ready to read, which may be headers or actual data
        virtual void ready_read(std::error_code &ec) override {
            bool response_done = false;

            if (current_response.headers.empty()) {
                const size_t start_search = std::max(response_buffer.size(), size_t(4)) - 4;

                skate::tcp_socket::read(ec, response_buffer, 1);

                // Detect double line ending to get mark end of headers
                if (response_buffer.find("\r\n\r\n", start_search) == response_buffer.npos)
                    return;

                current_response = read_headers(ec);
                response_buffer.clear();
            } else if (!response_chunked) { // Standard response body
                response_body_consumed += skate::tcp_socket::read(ec, response_buffer, response_body_expected - response_body_consumed);

                http_response_body_chunk(current_response, response_buffer);
                response_buffer.clear();

                response_done = response_body_consumed == response_body_expected;
            } else { // Chunked response body
                const size_t start_search = std::max(response_buffer.size(), size_t(2)) - 2;

                skate::tcp_socket::read(ec, response_buffer, 1);

                // Detect line ending to get mark end of chunk header
                if (response_buffer.find("\r\n", start_search) == response_buffer.npos)
                    return;
            }

            // If at end of body...
            if (response_done) {
                if ((current_response.major < 1 || (current_response.major == 1 && current_response.minor == 0)) ||
                     current_request.headers["Connection"].find("close") != std::string::npos) // HTTP 1.0 or "Connection: close", close connection
                    disconnect(ec);

                current_response = {};
            }
        }

        // Write callback handles chunked writing of a request datastream
        virtual void ready_write(std::error_code &ec) override {
            if (ec)
                return;

            if (current_request.bodystream) {
                char buffer[4096];

                const size_t read = current_request.bodystream->sgetn(buffer, sizeof(buffer));

                write(ec, to_string_hex(read));
                write(ec, "\r\n");

                if (read) {
                    write(ec, buffer, read);
                    write(ec, "\r\n");
                } else {
                    http_request_body_sent(current_request);
                    current_request.bodystream = nullptr;
                }
            }
        }

        virtual void error(std::error_code ec) override {
            std::cout << "ERROR: " << ec.message() << std::endl;
        }

    public:
        http_client_socket() {}
        virtual ~http_client_socket() {}

        virtual void http_request_body_sent(const http_request &/* request */) {}
        virtual void http_response_received(const http_response &response) {
            std::cout << "Version: HTTP/" << int(response.major) << '.' << int(response.minor) << '\n';
            std::cout << "   Code: " << response.code << '\n';
            std::cout << " Status: " << response.status << '\n';
            for (const auto &header: response.headers) {
                std::cout << " Header: \"" << header.first << "\": " << header.second << '\n';
            }
        }
        virtual void http_response_body_chunk(const http_response &response, const std::string &chunk) {
            std::cout << chunk;
        }

        // TODO: Read a single HTTP response from the socket (blocking mode only)
        http_response read_http_response_sync(std::error_code &ec) {
            if (ec)
                return {};
            else if (!is_blocking()) {
                ec = std::make_error_code(std::errc::operation_not_supported);
                return {};
            }

            do {
                skate::tcp_socket::read(ec, response_buffer, 1);
            } while (response_buffer.find("\r\n\r\n", std::max(response_buffer.size(), size_t(4)) - 4) == response_buffer.npos && !ec);

            auto headers = read_headers(ec);

            response_buffer.clear();

            return headers;
        }

        // Write a single HTTP request to the socket
        // Once the body has been written, http_request_body_sent() is called with the current request
        void write_http_request(std::error_code &ec, http_request request) {
            if (ec)
                return;

            std::string path = request.url.get_path_and_query(skate::url::encoding::percent);
            if (path.empty())
                path = "/";

            std::string txbuf;
            for (const auto c: request.method)
                txbuf += skate::toupper(c);

            txbuf += ' ' + path + " HTTP/" + std::to_string(request.major) + '.' + std::to_string(request.minor) + "\r\n";
            txbuf += "Host: " + request.url.get_hostname(skate::url::encoding::percent) + "\r\n";

            // Remove possibly user-defined headers
            request.headers.erase("Host");
            request.headers.erase("Content-Length");
            request.headers.erase("Transfer-Encoding");

            // Add necessary headers
            if (request.bodystream)
                request.headers["Transfer-Encoding"] = "chunked";
            else
                request.headers["Content-Length"] = std::to_string(request.body.size());

            for (const auto &header : request.headers) {
                txbuf += header.first;
                txbuf += ": ";
                txbuf += header.second;
                txbuf += "\r\n";
            }

            txbuf += "\r\n";

            std::cout << "txbuf:\n" << txbuf;

            write(ec, txbuf);
            // Write message body
            if (request.bodystream == nullptr) {
                write(ec, request.body);

                if (!ec)
                    http_request_body_sent(request);
            } else if (is_blocking()) {
                while (request.bodystream && !ec)
                    ready_write(ec);

                if (!ec)
                    http_request_body_sent(request);
            } else {
                current_request = std::move(request);
            }
        }
    };
}

#endif // SKATE_HTTP_H
