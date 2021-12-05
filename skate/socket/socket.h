/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_SOCKET_H
#define SKATE_SOCKET_H

#include "address.h"
#include "../io/buffer.h"
#include "../containers/abstract_list.h"

#include <array>
#include <memory>
#include <vector>
#include <deque>
#include <functional>
#include <chrono>
#include <cstddef>
#include <climits>
#include <algorithm>

#include <mutex>

#if POSIX_OS
# include <sys/ioctl.h>
#endif

// See https://beej.us/guide/bgnet/html

namespace skate {
    enum class socket_state {
        invalid,                    // Invalid, unconnected socket, not initialized yet and not available for reading or writing
        looking_up_host,            // Performing a host name lookup (DNS), not yet connected
        connecting,                 // Establishing a connection with a remote host
        connected,                  // Connected to a specific remote client
        bound,                      // Bound to a local port and idle
        disconnecting,              // Destroying connection with a remote host
        listening                   // Bound to a local port and listening for incoming connections
    };

    enum class socket_blocking {
        blocking,                   // A synchronous socket, blocks on IO
        nonblocking                 // An asynchronous socket, doesn't block on IO
    };

    enum class socket_type {
        unknown = 0,
        stream = SOCK_STREAM,       // A TCP-like stream socket (sequence of characters)
        datagram = SOCK_DGRAM       // A UDP-like datagram socket (sequence of messages)
    };

    enum class socket_protocol {
        unknown = 0,
        tcp = IPPROTO_TCP,          // standard TCP protocol
        udp = IPPROTO_UDP           // standard UDP protocol
    };

    class getaddrinfo_error_category_type : public std::error_category {
    public:
        virtual const char *name() const noexcept { return "getaddrinfo"; }
        virtual std::string message(int ev) const {
#if WINDOWS_OS
            static std::mutex gaierror_mtx;

            std::lock_guard<std::mutex> lock(gaierror_mtx);

            return to_utf8(static_cast<LPCWSTR>(::gai_strerrorW(ev)));
#elif POSIX_OS
            return ::gai_strerror(ev);
#else
# error Platform not supported
#endif
        }
    };

    inline const getaddrinfo_error_category_type &getaddrinfo_error_category() {
        static const getaddrinfo_error_category_type gai_category;
        return gai_category;
    }

#if POSIX_OS
    typedef int system_file_descriptor;
    typedef int system_socket_descriptor;

    inline const std::error_category &socket_category() {
        return std::system_category();
    }

    namespace impl {
        constexpr const system_socket_descriptor system_invalid_socket_value = -1;

        inline std::error_code socket_error() noexcept {
            const int v = errno;
            return std::error_code(v, socket_category());
        }

        inline std::error_code close_socket(system_socket_descriptor socket) noexcept {
            return std::error_code(::close(socket), socket_category());
        }

        inline bool socket_would_block(std::error_code code) noexcept {
            return code == std::error_code(EAGAIN, socket_category()) ||
                   code == std::error_code(EWOULDBLOCK, socket_category());
        }

        inline size_t socket_pending_read_bytes(std::error_code &ec, system_socket_descriptor sock) noexcept {
            int bytes = 0;
            if (!ec && ::ioctl(sock, FIONREAD, &bytes) < 0)
                ec = socket_error();

            return size_t(bytes);
        }

        inline void socket_set_blocking(std::error_code &ec, system_socket_descriptor sock, bool b) noexcept {
            if (!ec) {
                const int flags = ::fcntl(sock, F_GETFL);
                if (flags < 0 || ::fcntl(sock, F_SETFL, b? (flags & ~O_NONBLOCK): (flags | O_NONBLOCK)) < 0)
                    ec = socket_error();
            }
        }
    }

    enum class socket_shutdown {
        read = SHUT_RD,
        write = SHUT_WR,
        both = SHUT_RDWR
    };
#elif WINDOWS_OS
    typedef HANDLE system_file_descriptor;
    typedef SOCKET system_socket_descriptor;

    inline const std::error_category &socket_category() {
        return win32_category();
    }

    namespace impl {
        constexpr const system_socket_descriptor system_invalid_socket_value = INVALID_SOCKET;

        inline std::error_code socket_error() noexcept {
            const int v = ::WSAGetLastError();
            return std::error_code(v, win32_category());
        }

        inline std::error_code close_socket(system_socket_descriptor socket) noexcept {
            return std::error_code(::closesocket(socket), win32_category());
        }

        inline bool socket_would_block(std::error_code code) noexcept {
            return code == std::error_code(WSAEWOULDBLOCK, win32_category());
        }

        inline size_t socket_pending_read_bytes(std::error_code &ec, system_socket_descriptor sock) noexcept {
            ULONG bytes = 0;
            if (!ec && ::ioctlsocket(sock, FIONREAD, &bytes) < 0)
                ec = socket_error();

            return bytes;
        }

        // Sets whether this socket is blocking or asynchronous
        inline void socket_set_blocking(std::error_code &ec, system_socket_descriptor sock, bool b) noexcept {
            u_long opt = !b;
            if (!ec && ::ioctlsocket(sock, FIONBIO, &opt) < 0)
                ec = socket_error();
        }
    }

    enum class socket_shutdown {
        read = SD_RECEIVE,
        write = SD_SEND,
        both = SD_BOTH
    };
#else
# error Platform not supported
#endif

    // A base socket class
    // Subclasses must do the following:
    //   - Set socket::did_write to true immediately when the socket object is written to, whether or not the write actually succeeded
    //   - Reimplement all pure virtual functions
    class socket {
        socket(const socket &) = delete;
        socket(socket &&) = delete;
        socket &operator=(const socket &) = delete;
        socket &operator=(socket &&) = delete;

        template<typename>
        friend class socket_server;

        void do_server_connected(std::error_code &ec) {
            connected(ec);

            if (ec)
                error(ec);
        }
        void do_server_read(std::error_code &ec) {
            if (!ec)
                ready_read(ec);

            if (ec)
                error(ec);
        }
        void do_server_write(std::error_code &ec) {
            async_flush_write_buffer(ec);
            if (!ec)
                ready_write(ec);

            if (ec)
                error(ec);
        }
        void do_server_disconnected(std::error_code &ec) {
            disconnected(ec);

            if (ec)
                error(ec);
        }

    protected:
        socket(system_socket_descriptor sock, socket_state current_state, bool is_blocking) noexcept
            : did_write(false)
            , sock(sock)
            , s(current_state)
            , blocking(is_blocking)
        {}

        bool did_write; // Subclasses must set to true immediately when the socket object is written to (whether or not the write actually succeeded. stream_socket and datagram_socket already do this)

        virtual void connected(std::error_code &) {}
        virtual void ready_read(std::error_code &) {}
        virtual void ready_write(std::error_code &) {}
        virtual void disconnected(std::error_code &) {}
        virtual void error(std::error_code) {}

    public:
        socket() : socket(impl::system_invalid_socket_value, socket_state::invalid, true) {}
        virtual ~socket() {
            if (sock != impl::system_invalid_socket_value)
                impl::close_socket(sock);
        }

        system_socket_descriptor native() const noexcept { return sock; }

        socket_state state() const noexcept { return s; }
        bool is_null() const noexcept { return s == socket_state::invalid; }
        bool is_looking_up_host() const noexcept { return s == socket_state::looking_up_host; }
        bool is_connecting() const noexcept { return s == socket_state::connecting; }
        bool is_connected() const noexcept { return s == socket_state::connected; }
        bool is_bound() const noexcept { return s == socket_state::bound || is_listening(); }
        bool is_listening() const noexcept { return s == socket_state::listening; }

        bool is_blocking() const noexcept { return blocking; }

        // Returns remote address information (only if connected)
        // port is in native byte order, and is indeterminate if an error occurs
        socket_address remote_address(std::error_code &ec) const {
            if (ec)
                return {};

            if (!is_connected()) {
                ec = std::make_error_code(std::errc::not_connected);
                return {};
            }

            struct sockaddr_storage addr;
            socklen_t addrlen = sizeof(addr);

            const int err = ::getpeername(sock, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
            if (err) {
                ec = std::error_code(err, socket_category());
                return {};
            } else {
                ec.clear();
                return socket_address(reinterpret_cast<const struct sockaddr *>(&addr));
            }
        }

        // Returns local address information (only if connected or bound)
        // port is in native byte order, and is indeterminate if an error occurs
        socket_address local_address(std::error_code &ec) const {
            if (ec)
                return {};

            if (!is_connected() && !is_bound()) {
                ec = std::make_error_code(std::errc::not_connected);
                return {};
            }

            struct sockaddr_storage addr;
            socklen_t addrlen = sizeof(addr);

            const int err = ::getsockname(sock, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
            if (err) {
                ec = std::error_code(err, socket_category());
                return {};
            } else {
                ec.clear();
                return socket_address(reinterpret_cast<const struct sockaddr *>(&addr));
            }
        }

        // Starts socket listening for connections
        void listen(std::error_code &ec, int backlog = SOMAXCONN) {
            if (ec)
                return;

            if (state() != socket_state::bound) {
                ec = std::make_error_code(std::errc::not_connected);
                return;
            }

            ec = std::error_code(::listen(sock, backlog), socket_category());
            if (!ec)
                s = socket_state::listening;
        }

        // Shuts down either the read part of the socket, the write part, or both
        void shutdown(std::error_code &ec, socket_shutdown type = socket_shutdown::both) {
            if (ec || (!is_connected() && !is_bound()))
                return;

            ec = std::error_code(::shutdown(sock, static_cast<int>(type)), socket_category());
        }

        // Closes the socket and ensures the socket is in an invalid state
        void disconnect(std::error_code &ec) {
            if (!ec && sock != impl::system_invalid_socket_value) {
                ec = impl::close_socket(sock);
                sock = impl::system_invalid_socket_value;
                s = socket_state::invalid;

                if (is_blocking() && !ec)
                    disconnected(ec);
            }
        }

        // Sets whether the socket is blocking (true) or asynchronous (false)
        void set_blocking(std::error_code &ec, bool b) noexcept {
            if (ec)
                return;

            if (sock == impl::system_invalid_socket_value) {
                blocking = b;
            } else {
                impl::socket_set_blocking(ec, sock, b);
                if (!ec)
                    blocking = b;
            }
        }

        // Connect synchronously to any specified address
        void connect_sync(std::error_code &ec, const std::vector<socket_address> &remote) {
            if (ec)
                return;
            else if (remote.empty()) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            }

            for (const auto &address: remote) {
                connect_sync(ec, address);
                if (!ec) {
                    connected(ec);
                    return; // Success!
                }
                ec.clear();
            }
        }

        // Bind to any specified address
        void bind(std::error_code &ec, const std::vector<socket_address> &local) {
            if (ec)
                return;
            else if (local.empty()) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            }

            for (const auto &address: local) {
                bind(ec, address);
                if (!ec)
                    return; // Success!
                ec.clear();
            }
        }

        // Connect synchronously to an external address
        virtual void connect_sync(std::error_code &ec, socket_address remote) = 0;

        // Bind to a local address
        virtual void bind(std::error_code &ec, socket_address local) = 0;

        // Returns the socket type that this socket supports
        virtual socket_type type() const noexcept = 0;
        // Returns the protocol that this socket supports
        virtual socket_protocol protocol() const noexcept = 0;
        // Attempt to fill the read buffer from the native socket
        virtual void async_fill_read_buffer(std::error_code &ec) = 0;
        // Attempt to flush the write buffer to the native socket
        virtual void async_flush_write_buffer(std::error_code &ec) = 0;
        // Must return true if more data is waiting to be read
        virtual bool async_pending_read() const = 0;
        // Must return true if more data is waiting to be written
        virtual bool async_pending_write() const = 0;
        // Must return true if read stream is closed
        virtual bool async_closed_read() const = 0;
        // Must return true if write stream is closed
        virtual bool async_closed_write() const = 0;

        // Factory to create another socket with this type and protocol, specifically for accepting new connections
        // May return null if creating a new socket is not supported (the default)
        virtual std::unique_ptr<socket> create(system_socket_descriptor /*desc*/, socket_state /*current_state*/, bool /*blocking*/) = 0;

        // Synchronous name resolution
        std::vector<socket_address> resolve(std::error_code &ec, const network_address &address, address_type addrtype = address_type::ip_address_unspecified) const {
            if (ec)
                return {};

            std::vector<socket_address> result;

            struct addrinfo hints;
            struct addrinfo *addresses = nullptr, *ptr = nullptr;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = addrtype;
            hints.ai_socktype = static_cast<int>(type());
            hints.ai_protocol = static_cast<int>(protocol());
            hints.ai_flags = AI_PASSIVE;

            const int err = ::getaddrinfo(address.is_null()? NULL: address.to_string(false).c_str(),
                                          address.port()? std::to_string(address.port()).c_str(): NULL,
                                          &hints,
                                          &addresses);

#if POSIX_OS
            if (err == EAI_SYSTEM) {
                ec = impl::socket_error();
                return result;
            } else
#endif
            if (err) {
                ec = std::error_code(err, getaddrinfo_error_category());
                return result;
            }

            try {
                for (ptr = addresses; ptr; ptr = ptr->ai_next) {
                    switch (ptr->ai_family) {
                        default: continue;
                        case ip_address_v4:
                        case ip_address_v6: break;
                    }

                    result.push_back(socket_address(ptr->ai_addr));
                }
            } catch (...) {
                freeaddrinfo(addresses);
                throw;
            }

            freeaddrinfo(addresses);

            return result;
        }

    protected:
        // Returns the number of bytes waiting to be read without blocking
        size_t direct_read_pending_bytes(std::error_code &ec) noexcept {
            if (sock == impl::system_invalid_socket_value || is_blocking())
                return 0;

            return impl::socket_pending_read_bytes(ec, sock);
        }

        system_socket_descriptor sock;
        socket_state s;
        bool blocking;
    };

    class stream_socket : public socket {
        constexpr static const size_t READ_BUFFER_SIZE = 4096;

    protected:
        stream_socket(system_socket_descriptor s, socket_state current_state, bool is_blocking)
            : socket(s, current_state, is_blocking)
        {}

    public:
        stream_socket() {}
        virtual ~stream_socket() {}

        virtual socket_type type() const noexcept final { return socket_type::stream; }

        void set_read_limit(size_t limit) noexcept { read_buffer.set_max_size(limit); }

        // Read data from the socket, up to max bytes, and returns the number of bytes read
        // If the socket is blocking it will wait until exactly max bytes are read, unless an error occurs
        // Reads any data that was buffered in the socket first, then reads directly from the socket
        // The error code is set to success if the socket couldn't provide more data immediately
        size_t read(std::error_code &ec, char *data, size_t max) {
            if (ec)
                return 0;

            size_t bytes_read = read_buffer.read(max, [&](const char *d, size_t l) {
                memcpy(data, d, l);
                data += l;
                max -= l;
                return l;
            });

            // data and max are now updated for direct reading if more needs to be read
            if (max)
                bytes_read += direct_read(ec, data, max);

            return bytes_read;
        }

        // Read data from the socket, up to max bytes, and appends it to a container, and returns the number of bytes read
        // If the socket is blocking it will wait until the exact number of bytes are read, unless an error occurs
        // Reads any data that was buffered in the socket first, then reads directly from the socket
        // The error code is set to success if the socket couldn't provide more data immediately
        template<typename Container>
        size_t read(std::error_code &ec, Container &c, size_t max) {
            if (ec)
                return 0;

            // First read from read buffer and reduce remaining max
            size_t total_bytes_read = read_buffer.read_into(max, abstract::back_inserter(c));
            max -= total_bytes_read;

            if (max) {
                // Then read directly from the socket into a temporary buffer
                std::array<char, READ_BUFFER_SIZE> buf;

                size_t bytes_read = 0;
                do {
                    bytes_read = direct_read(ec, buf.data(), std::min(buf.size(), max));

                    // Copy what was read into buf into the container and reduce remaining max
                    std::copy(buf.data(), buf.data() + bytes_read, abstract::back_inserter(c));
                    total_bytes_read += bytes_read;
                    max -= bytes_read;
                } while (bytes_read == buf.size() && !ec); // If everything was read and no error, try again
            }

            return total_bytes_read;
        }

        // Read data from the socket, up to max bytes, and append it to a new container
        // If the socket is blocking it will wait until the exact number of bytes are read, unless an error occurs
        // Reads any data that was buffered in the socket first, then reads directly from the socket
        // The error code is set to success if the socket couldn't provide more data immediately
        template<typename Container = std::string>
        Container read(std::error_code &ec, size_t max) {
            Container c;
            read(ec, c, max);
            return c;
        }

        // Read data from the socket until no more data is available or the socket is closed by the remote host
        // Data is appended to the provided container, and the number of bytes written is returned
        // If the socket is blocking it will wait until the remote host closes the connection, unless an error occurs
        // Reads any data that was buffered in the socket first, then reads directly from the socket
        template<typename Container>
        size_t read_all(std::error_code &ec, Container &c) {
            return read(ec, c, SIZE_MAX);
        }

        // Read data from the socket until no more data is available or the socket is closed by the remote host
        // Data is appended to a new container, which is then returned
        // If the socket is blocking it will wait until the remote host closes the connection, unless an error occurs
        // Reads any data that was buffered in the socket first, then reads directly from the socket
        template<typename Container = std::string>
        Container read_all(std::error_code &ec) {
            Container c;
            read_all(ec, c);
            return c;
        }

        // Attempts to fill the read buffer with as much data as is available on the socket
        virtual void async_fill_read_buffer(std::error_code &ec) override {
            if (ec || is_blocking())
                return;

            // Read directly from the socket into a temporary buffer
            std::array<char, READ_BUFFER_SIZE> buf;

            size_t bytes_read = 0;
            do {
                bytes_read = direct_read(ec, buf.data(), std::min(buf.size(), read_buffer.free_space()));

                read_buffer.write(buf.data(), buf.data() + bytes_read);
            } while (bytes_read == buf.size() && !ec);
        }

        // Write data to the socket
        // The data is buffered for writing later if an error occurs or if the socket is asynchronous and couldn't accept more data immediately
        // The error code is set to success if the socket couldn't accept more data immediately
        void write(std::error_code &ec, const char *data, size_t len) {
            if (ec)
                return;

            const size_t buffered = write_buffer.size();
            size_t written_from_new_buffer = 0;

            socket::did_write = true;

            // Did all of write_buffer get sent?
            if (write_buffer.read_all([&](const char *data, size_t len) { return direct_write(ec, data, len); }) == buffered) {
                // ec must be valid if all data was written

                // Attempt direct write of new data
                written_from_new_buffer = direct_write(ec, data, len);
            }

            // Any data that didn't get sent, add to the write buffer
            if (written_from_new_buffer != len)
                write_buffer.write(data + written_from_new_buffer, data + len);
        }
        void write(std::error_code &ec, const char *str) {
            write(ec, str, strlen(str));
        }
        void write(std::error_code &ec, const std::string &str) {
            write(ec, str.c_str(), str.size());
        }
        void put(std::error_code &ec, char c) {
            write(ec, &c, 1);
        }

        // Attempt to flush the write buffer
        virtual void async_flush_write_buffer(std::error_code &ec) override {
            if (write_buffer.size())
                write(ec, nullptr, 0);
        }

        virtual bool async_pending_read() const override { return read_buffer.size(); }
        virtual bool async_closed_read() const override { return read_buffer.is_closed(); }
        virtual bool async_pending_write() const override { return write_buffer.size(); }
        virtual bool async_closed_write() const override { return write_buffer.is_closed(); }

        using socket::connect_sync; // Import so overloads are available, see https://stackoverflow.com/questions/1628768/why-does-an-overridden-function-in-the-derived-class-hide-other-overloads-of-the?rq=1
        virtual void connect_sync(std::error_code &ec, socket_address remote) override {
            if (ec)
                return;

            if (!is_null() && !is_bound()) {
                ec = std::make_error_code(std::errc::device_or_resource_busy);
                return;
            }

            direct_bind(ec, remote, true);
        }

        using socket::bind; // Import so overloads are available, see https://stackoverflow.com/questions/1628768/why-does-an-overridden-function-in-the-derived-class-hide-other-overloads-of-the?rq=1
        virtual void bind(std::error_code &ec, socket_address local) override {
            if (ec)
                return;

            if (!is_null()) {
                ec = std::make_error_code(std::errc::device_or_resource_busy);
                return;
            }

            direct_bind(ec, local, false);
        }

        size_t write_bytes_pending() const noexcept { return write_buffer.size(); }
        size_t read_bytes_pending() const noexcept { return read_buffer.size(); }

    protected:
        // Synchronously binds to a local address or connects to a remote address
        void direct_bind(std::error_code &ec, socket_address address, bool address_is_remote) noexcept {
            switch (address.type()) {
                case ip_address_v4:
                case ip_address_v6:
                    break;
                default:
                    ec = std::make_error_code(std::errc::invalid_argument);
                    return;
            }

            const bool new_socket_required = is_null();
            const int yes = 1;

            if (new_socket_required && (sock = ::socket(address.type(),
                                                        static_cast<int>(socket_type::stream),
                                                        static_cast<int>(protocol()))) == impl::system_invalid_socket_value) {
                ec = impl::socket_error();
            } else if (address_is_remote? ::connect(sock, address.native(), address.native_length()) < 0:
                       (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes)) < 0 ||
                        ::bind(sock, address.native(), address.native_length()) < 0)) {
                ec = impl::socket_error();

                if (new_socket_required) {
                    impl::close_socket(sock);
                    sock = impl::system_invalid_socket_value;
                }
            } else {
                s = address_is_remote? socket_state::connected: socket_state::bound;

                // Prior to this call, the blocking mode could have been set but there was no descriptor to set it on
                // This needs to be done now
                if (new_socket_required)
                    set_blocking(ec, is_blocking());
            }
        }

        // Read data directly from the socket, and return the number of bytes read
        // Returns a "would block" error if the operation would block
        size_t direct_read(std::error_code &ec, char *data, size_t max) {
            if (!is_connected() && !is_bound()) {
                ec = std::make_error_code(std::errc::not_connected);
                return 0;
            }

            const size_t original_max = max;
            while (max) {
                const int to_read = max < INT_MAX? static_cast<int>(max): INT_MAX;
                const int read = ::recv(sock, data, to_read, 0);
                if (read == 0) {
                    return original_max - max;
                } else if (read < 0) {
                    ec = impl::socket_error();

                    // Ignore "would block" errors for stream sockets
                    if (impl::socket_would_block(ec))
                        ec.clear();

                    return original_max - max;
                }

                data += read;
                max -= read;
            }

            ec.clear();
            return original_max;
        }

        // Write data directly to the socket, and return the number of bytes written
        // Returns a "would block" error if the operation would block
        size_t direct_write(std::error_code &ec, const char *data, size_t len) {
            if (!is_connected() && !is_bound()) {
                ec = std::make_error_code(std::errc::not_connected);
                return 0;
            }

            const size_t original_size = len;
            while (len) {
                const int to_send = len < INT_MAX? static_cast<int>(len): INT_MAX;
                const int sent = ::send(sock, data, to_send, MSG_NOSIGNAL);
                if (sent < 0) {
                    ec = impl::socket_error();

                    // Ignore "would block" errors for stream sockets
                    if (impl::socket_would_block(ec))
                        ec.clear();

                    return original_size - len;
                }

                data += sent;
                len -= sent;
            }

            ec.clear();
            return original_size;
        }

        io_buffer<char> write_buffer; // Outgoing buffer awaiting sending
        io_buffer<char> read_buffer;  // Incoming buffer awaiting reading
    };

    class socket_datagram {
        bool valid;
        std::string d;
        socket_address remote;

    public:
        socket_datagram() : valid(false) {}
        socket_datagram(std::string data, socket_address remote) : valid(true), d(data), remote(remote) {}

        bool is_valid() const noexcept { return valid; }
        const std::string &data() const noexcept { return d; }
        const socket_address &remote_address() const noexcept { return remote; }

        void clear_data() {
            valid = false;
            d = {};
        }

        void set_data(std::string data) {
            valid = true;
            d = std::move(data);
        }

        void set_remote_address(socket_address a) {
            remote = a;
        }
    };

    class datagram_socket : public socket {
    public:
        virtual ~datagram_socket() {}

        virtual socket_type type() const noexcept final { return socket_type::datagram; }

        void set_read_limit(size_t packets) noexcept { read_buffer.set_max_size(packets); }

        socket_datagram read_datagram(std::error_code &ec) {
            if (ec)
                return {};

            socket_datagram result;

            if (!read_buffer.read(result))
                result = direct_read(ec);

            return result;
        }

        virtual void async_fill_read_buffer(std::error_code &ec) override {
            if (ec || is_blocking())
                return;

            // Read directly from the socket into a temporary buffer
            do {
                socket_datagram datagram = direct_read(ec);

                if (!ec)
                    read_buffer.write(datagram); // May silently drop packets if limited-size buffer and not enough space
            } while (!ec);

            if (impl::socket_would_block(ec))
                ec.clear();
        }

        void write_datagram(std::error_code &ec, std::string datagram, bool queue_on_error = true) {
            if (ec)
                return;

            socket::did_write = true;
            async_flush_write_buffer(ec);

            if (!ec) { // No errors sending buffered packets, try new packet
                direct_write(ec, datagram.c_str(), datagram.size());
                if (!ec)
                    return;
            }

            if (queue_on_error || impl::socket_would_block(ec)) {
                write_buffer.write(socket_datagram(std::move(datagram), socket_address{}));

                if (impl::socket_would_block(ec))
                    ec.clear();
            }
        }

        void write_datagram(std::error_code &ec, const socket_address &remote, std::string datagram, bool queue_on_error = true) {
            if (ec)
                return;

            socket::did_write = true;
            async_flush_write_buffer(ec);

            if (!ec) { // No errors sending buffered packets, try new packet
                direct_write_to(ec, datagram.c_str(), datagram.size(), remote);
                if (!ec)
                    return;
            }

            if (queue_on_error || impl::socket_would_block(ec)) {
                write_buffer.write(socket_datagram(std::move(datagram), remote));

                if (impl::socket_would_block(ec))
                    ec.clear();
            }
        }

        virtual void async_flush_write_buffer(std::error_code &ec) override {
            if (ec)
                return;

            write_buffer.read_all([&](const socket_datagram *datagrams, size_t total_datagrams) {
                for (size_t i = 0; i < total_datagrams; ++i) {
                    if (datagrams[i].remote_address().is_unspecified())
                        direct_write(ec, datagrams[i].data().c_str(), datagrams[i].data().size());
                    else
                        direct_write_to(ec, datagrams[i].data().c_str(), datagrams[i].data().size(), datagrams[i].remote_address());

                    if (ec)
                        return i; // Error occured, so break early. Return number of packets actually sent
                }

                return total_datagrams;
            });

            if (impl::socket_would_block(ec))
                ec.clear();
        }

        virtual bool async_pending_read() const override { return read_buffer.size(); }
        virtual bool async_closed_read() const override { return read_buffer.is_closed(); }
        virtual bool async_pending_write() const override { return write_buffer.size(); }
        virtual bool async_closed_write() const override { return write_buffer.is_closed(); }

        using socket::connect_sync; // Import so overloads are available, see https://stackoverflow.com/questions/1628768/why-does-an-overridden-function-in-the-derived-class-hide-other-overloads-of-the?rq=1
        virtual void connect_sync(std::error_code &ec, socket_address remote) override {
            if (ec)
                return;

            if (!is_null() && !is_bound()) {
                ec = std::make_error_code(std::errc::device_or_resource_busy);
                return;
            }

            direct_bind(ec, remote, true);
        }

        using socket::bind; // Import so overloads are available, see https://stackoverflow.com/questions/1628768/why-does-an-overridden-function-in-the-derived-class-hide-other-overloads-of-the?rq=1
        virtual void bind(std::error_code &ec, socket_address local) override {
            if (ec)
                return;

            if (!is_null()) {
                ec = std::make_error_code(std::errc::device_or_resource_busy);
                return;
            }

            direct_bind(ec, local, false);
        }

    protected:
        void direct_bind(std::error_code &ec, socket_address address, bool address_is_remote) noexcept {
            switch (address.type()) {
                case ip_address_v4:
                case ip_address_v6:
                    break;
                default:
                    ec = std::make_error_code(std::errc::invalid_argument);
                    return;
            }

            const bool new_socket_required = is_null();
            const int yes = 1;

            if (new_socket_required && (sock = ::socket(address.type(),
                                                        static_cast<int>(socket_type::datagram),
                                                        static_cast<int>(protocol()))) == impl::system_invalid_socket_value) {
                ec = impl::socket_error();
            } else if ((address.is_ipv4() && ::setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&yes), sizeof(yes)) < 0) ||
                       (address_is_remote? ::connect(sock, address.native(), address.native_length()) < 0:
                                           ::bind(sock, address.native(), address.native_length()) < 0)) {
                ec = impl::socket_error();

                if (new_socket_required) {
                    impl::close_socket(sock);
                    sock = impl::system_invalid_socket_value;
                }
            } else {
                s = address_is_remote? socket_state::connected: socket_state::bound;

                // Prior to this call, the blocking mode could have been set but there was no descriptor to set it on
                // This needs to be done now
                if (new_socket_required)
                    set_blocking(ec, is_blocking());
            }
        }

        socket_datagram direct_read(std::error_code &ec) {
            if (!is_connected() && !is_bound()) {
                ec = std::make_error_code(std::errc::not_connected);
                return {};
            }

            socket_address remote;
            std::string data;

            {
                std::array<char, 65536> buf;

                const size_t bytes_read = direct_read_from(ec, buf.data(), buf.size(), remote);

                data = std::string(buf.data(), bytes_read);
            }

            if (ec)
                return {};
            else
                return socket_datagram(std::move(data), remote);
        }

        // Read data directly from the socket, retrieving the remote address, and return the number of bytes read
        // Returns a "would block" error if the operation would block, and "message truncated" if the message couldn't fit in the buffer
        size_t direct_read_from(std::error_code &ec, char *data, size_t max, socket_address &remote) {
            struct sockaddr_storage remote_addr;
            socklen_t remote_addr_len = sizeof(remote_addr);

            const int read = ::recvfrom(sock, data, max < INT_MAX? int(max): INT_MAX, 0, reinterpret_cast<struct sockaddr *>(&remote_addr), &remote_addr_len);
            if (read < 0) {
                ec = impl::socket_error();
                return 0;
            } else {
                remote = socket_address(remote_addr);
                ec.clear();
                return read;
            }
        }

        // Write data directly to the socket, and return the number of bytes written
        // Returns a "would block" error if the operation would block
        size_t direct_write(std::error_code &ec, const char *data, size_t len) {
            if (!is_connected() && !is_bound()) {
                ec = std::make_error_code(std::errc::not_connected);
                return 0;
            }

            if (len > INT_MAX) {
                ec = std::make_error_code(std::errc::message_size);
                return 0;
            }

            const int sent = ::send(sock, data, int(len), 0);
            if (sent < 0) {
                ec = impl::socket_error();
                return 0;
            } else {
                ec.clear();
                return sent;
            }
        }

        // Write data directly to the socket, and return the number of bytes written
        // Returns a "would block" error if the operation would block
        size_t direct_write_to(std::error_code &ec, const char *data, size_t len, const socket_address &remote) {
            if (!is_bound()) {
                ec = std::make_error_code(std::errc::not_connected);
                return 0;
            }

            if (len > INT_MAX) {
                ec = std::make_error_code(std::errc::message_size);
                return 0;
            }

            const int sent = ::sendto(sock, data, int(len), 0, remote.native(), remote.native_length());
            if (sent < 0) {
                ec = impl::socket_error();
                return 0;
            } else {
                ec.clear();
                return sent;
            }
        }

        io_buffer<socket_datagram> write_buffer; // Outgoing buffer awaiting sending; if address is unspecified, use write_datagram() instead of write_datagram_to()
        io_buffer<socket_datagram> read_buffer;  // Incoming buffer awaiting reading
    };

    class tcp_socket : public stream_socket {
    protected:
        tcp_socket(system_socket_descriptor s, socket_state current_state, bool is_blocking)
            : stream_socket(s, current_state, is_blocking)
        {}

    public:
        tcp_socket() {}
        virtual ~tcp_socket() {}

        virtual socket_protocol protocol() const noexcept override { return socket_protocol::tcp; }

        virtual std::unique_ptr<socket> create(system_socket_descriptor desc, socket_state current_state, bool is_blocking) override { return std::unique_ptr<socket>{new tcp_socket(desc, current_state, is_blocking)}; }
    };

    class udp_socket : public datagram_socket {
    public:
        virtual ~udp_socket() {}

        virtual socket_protocol protocol() const noexcept override { return socket_protocol::udp; }

        virtual std::unique_ptr<socket> create(system_socket_descriptor, socket_state, bool) override { return {}; }
    };
}

#endif // SKATE_SOCKET_H
