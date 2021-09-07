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
        http_response() : code(200), major(1), minor(1) {}

        short code;
        unsigned char major, minor;
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
        std::string response;
        http_request current_request;

    protected:
        http_client_socket(skate::system_socket_descriptor desc, skate::socket_state current_state, bool blocking)
            : tcp_socket(desc, current_state, blocking)
        {}

        virtual void ready_read(std::error_code &ec) override {
            skate::tcp_socket::read_all(ec, response);

            if (response.find("\r\n\r\n") == response.npos)
                return;

            std::cout << response;
            disconnect(ec);
        }

        // Write callback handles chunked writing of a request datastream
        virtual void ready_write(std::error_code &ec) override {
            if (!is_blocking() && current_request.bodystream) {
                char buffer[4096];

                const size_t read = current_request.bodystream->sgetn(buffer, sizeof(buffer));

                write(ec, to_string_hex(read));
                if (!ec)
                    write(ec, "\r\n");

                if (read) {
                    if (!ec)
                        write(ec, buffer, read);
                    if (!ec)
                        write(ec, "\r\n");
                } else {
                    http_request_body_sent(current_request);
                    current_request.bodystream = nullptr;
                }
            }
        }

        virtual void error(std::error_code ec) override {
            std::cout << ec.message() << std::endl;
        }

    public:
        http_client_socket() {}
        virtual ~http_client_socket() {}

        virtual void http_request_body_sent(const http_request &/* request */) {}
        virtual void http_response_received(http_response &&/* response */) {}

        // Write a single HTTP request to the socket
        // Once the body has been written, http_request_body_sent() is called with the current request
        std::error_code write_http_request(http_request &&request) {
            std::error_code ec;

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

            if (!ec) {
                // Write message body
                if (request.bodystream == nullptr) {
                    write(ec, request.body);

                    if (!ec)
                        http_request_body_sent(request);
                }
            }

            current_request = std::move(request);

            return ec;
        }
    };
}

#endif // SKATE_HTTP_H
