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

    struct http_response {
        http_response() : major(1), minor(1), code(200) {}

        unsigned char major, minor;
        short code;
        std::string status;

        skate::url url;
        std::map<std::string, std::string, impl::less_case_insensitive> headers;
    };

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

    class http_server_socket : public skate::tcp_socket {
        std::string request;

    protected:
        http_server_socket(skate::system_socket_descriptor desc, skate::socket_state current_state, bool blocking)
            : tcp_socket(desc, current_state, blocking)
        {}

        virtual std::unique_ptr<skate::socket> create(skate::system_socket_descriptor desc, skate::socket_state current_state, bool is_blocking) override {
            return std::unique_ptr<skate::socket>{new http_server_socket(desc, current_state, is_blocking)};
        }

        virtual void ready_read(std::error_code &ec) override {
            request += read_all(ec);

            if (request.find("\r\n\r\n") == request.npos)
                return;

            disconnect(ec);
        }

        virtual void ready_write(std::error_code &) override {

        }

        virtual void error(std::error_code ec) override {
            std::cout << ec.message() << std::endl;
        }

    public:
        http_server_socket() {}
        virtual ~http_server_socket() {}

        virtual void http_request_received(http_request &&/* request */) {}
    };

    class http_client_socket : public skate::tcp_socket {
        std::string response_buffer;

        http_response current_response;
        http_request current_request;

        http_response read_headers(std::error_code &ec) {
            if (ec)
                return {};

            // At this point, all headers are read and waiting for parsing
            // Parse the headers now

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
                response.major = strtol(status, &end, 10);
                status = end;

                if (*status++ != '.') {
                    ec = std::make_error_code(std::errc::bad_message);
                    return {};
                }

                // Get HTTP minor version
                response.minor = strtol(status, &end, 10);
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
                while (response_buffer[colon] == ' ')
                    ++colon;

                std::string value{response_buffer.c_str() + colon, end_of_line - colon};

                response.headers[std::move(key)] = std::move(value);

                status = response_buffer.c_str() + end_of_line + 2;
            }

            http_response_received(response);

            return response;
        }

    protected:
        http_client_socket(skate::system_socket_descriptor desc, skate::socket_state current_state, bool blocking)
            : tcp_socket(desc, current_state, blocking)
        {}

        virtual void ready_read(std::error_code &ec) override {
            if (current_response.headers.empty()) {
                const size_t start_search = std::max(response_buffer.size(), size_t(4)) - 4;

                if (is_blocking())
                    skate::tcp_socket::read(ec, response_buffer, 1);
                else
                    skate::tcp_socket::read_all(ec, response_buffer);

                // Detect double line ending to get mark end of headers
                if (response_buffer.find("\r\n\r\n", start_search) == response_buffer.npos)
                    return;

                current_response = read_headers(ec);
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

            std::string txbuf = request.method + ' ' + path + " HTTP/" + std::to_string(request.major) + '.' + std::to_string(request.minor) + "\r\n";
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
