#ifndef SKATE_SOCKET_H
#define SKATE_SOCKET_H

#include "address.h"
#include "../io/buffer.h"

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
            if (::ioctl(sock, FIONREAD, &bytes) < 0)
                ec = socket_error();
            else
                ec.clear();

            return size_t(bytes);
        }

        inline void socket_set_blocking(std::error_code &ec, system_socket_descriptor sock, bool b) noexcept {
            const int flags = ::fcntl(sock, F_GETFL);
            if (flags < 0 || ::fcntl(sock, F_SETFL, b? (flags & ~O_NONBLOCK): (flags | O_NONBLOCK)) < 0)
                ec = socket_error();
            else
                ec.clear();
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
            if (::ioctlsocket(sock, FIONREAD, &bytes) < 0)
                ec = socket_error();
            else
                ec.clear();

            return bytes;
        }

        // Sets whether this socket is blocking or asynchronous
        inline void socket_set_blocking(std::error_code &ec, system_socket_descriptor sock, bool b) noexcept {
            u_long opt = !b;
            if (::ioctlsocket(sock, FIONBIO, &opt) < 0)
                ec = socket_error();
            else
                ec.clear();
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

    class socket {
        socket(const socket &) = delete;
        socket(socket &&) = delete;
        socket &operator=(const socket &) = delete;
        socket &operator=(socket &&) = delete;

    protected:
        constexpr socket(system_socket_descriptor sock, socket_state current_state, bool is_blocking) noexcept
            : sock(sock)
            , s(current_state)
            , blocking(is_blocking)
        {}

    public:
        constexpr socket() noexcept : sock(impl::system_invalid_socket_value), s(socket_state::invalid), blocking(true) {}
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
            if (!is_connected())
                throw std::logic_error("Socket can only use remote_address() if connected");

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
            if (!is_connected() && !is_bound())
                throw std::logic_error("Socket can only use local_address() if connected or bound");

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
            if (state() != socket_state::bound)
                throw std::logic_error("Socket can only use listen() if bound to an address");

            ec = std::error_code(::listen(sock, backlog), socket_category());
            if (!ec)
                s = socket_state::listening;
        }

        // Shuts down either the read part of the socket, the write part, or both
        void shutdown(std::error_code &ec, socket_shutdown type = socket_shutdown::both) {
            if (!is_connected() && !is_bound())
                throw std::logic_error("Socket can only use shutdown() if connected or bound to an address");

            ec = std::error_code(::shutdown(sock, static_cast<int>(type)), socket_category());
        }

        // Closes the socket and ensures the socket is in an invalid state
        void disconnect(std::error_code &ec) {
            if (sock != impl::system_invalid_socket_value) {
                ec = impl::close_socket(sock);
                sock = impl::system_invalid_socket_value;
                s = socket_state::invalid;
            } else
                ec.clear();
        }

        // Sets whether the socket is blocking (true) or asynchronous (false)
        void set_blocking(std::error_code &ec, bool b) noexcept {
            if (sock == impl::system_invalid_socket_value) {
                blocking = b;
                ec.clear();
            } else {
                impl::socket_set_blocking(ec, sock, b);
                if (!ec)
                    blocking = b;
            }
        }

        // Connect synchronously to any specified address
        void connect_sync(std::error_code &ec, const std::vector<socket_address> &remote) {
            if (remote.empty()) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            }

            for (const auto &address: remote) {
                connect_sync(ec, address);
                if (!ec)
                    return; // Success!
            }
        }

        // Bind to any specified address
        void bind(std::error_code &ec, const std::vector<socket_address> &local) {
            if (local.empty()) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            }

            for (const auto &address: local) {
                bind(ec, address);
                if (!ec)
                    return; // Success!
            }
        }

        // Connect synchronously to an external address
        virtual void connect_sync(std::error_code &ec, socket_address remote) = 0;

        // Bind to a local address
        virtual void bind(std::error_code &ec, socket_address local) = 0;

        virtual socket_type type() const noexcept = 0;
        virtual socket_protocol protocol() const noexcept = 0;
        virtual void async_fill_read_buffer(std::error_code &ec) = 0;
        virtual void async_flush_write_buffer(std::error_code &ec) = 0;

        // Synchronous name resolution
        std::vector<socket_address> resolve(std::error_code &ec, const network_address &address, address_type addrtype = address_type::ip_address_unspecified) const {
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

            ec.clear();
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

    public:
        virtual ~stream_socket() {}

        virtual socket_type type() const noexcept final { return socket_type::stream; }

        void set_read_limit(size_t limit) noexcept { read_buffer.set_max_size(limit); }

        // Read data from the socket, up to max bytes, and returns the number of bytes read
        // If the socket is blocking it will wait until exactly max bytes are read, unless an error occurs
        // Reads any data that was buffered in the socket first, then reads directly from the socket
        // The error code is set to success if the socket couldn't provide more data immediately
        size_t read(std::error_code &ec, char *data, size_t max) {
            size_t bytes_read = read_buffer.read(max, [&](const char *d, size_t l) {
                memcpy(data, d, l);
                data += l;
                max -= l;
                return l;
            });

            // data and max are now updated for direct reading if more needs to be read
            if (max)
                bytes_read += direct_read(ec, data, max);
            else
                ec.clear();

            return bytes_read;
        }

        // Read data from the socket, up to max bytes, and appends it to a container, and returns the number of bytes read
        // If the socket is blocking it will wait until the exact number of bytes are read, unless an error occurs
        // Reads any data that was buffered in the socket first, then reads directly from the socket
        // The error code is set to success if the socket couldn't provide more data immediately
        template<typename Container>
        size_t read(std::error_code &ec, Container &c, size_t max) {
            // First read from read buffer and reduce remaining max
            size_t total_bytes_read = read_buffer.read_into(max, std::back_inserter(c));
            max -= total_bytes_read;

            if (max) {
                // Then read directly from the socket into a temporary buffer
                std::array<char, READ_BUFFER_SIZE> buf;

                size_t bytes_read = 0;
                do {
                    bytes_read = direct_read(ec, buf.data(), std::min(buf.size(), max));

                    // Copy what was read into buf into the container and reduce remaining max
                    std::copy(buf.data(), buf.data() + bytes_read, std::back_inserter(c));
                    total_bytes_read += bytes_read;
                    max -= bytes_read;
                } while (bytes_read == buf.size() && !ec); // If everything was read and no error, try again
            } else
                ec.clear();

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
        template<typename Container>
        Container read_all(std::error_code &ec) {
            Container c;
            read_all(ec, c);
            return c;
        }

        // Attempts to fill the read buffer with as much data as is available on the socket
        virtual void async_fill_read_buffer(std::error_code &ec) override {
            if (is_blocking()) {
                ec.clear();
                return;
            }

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
            const size_t buffered = write_buffer.size();
            size_t written_from_new_buffer = 0;

            ec.clear();

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
        void write(std::error_code &ec, const char *s) {
            write(ec, s, strlen(s));
        }
        void write(std::error_code &ec, const std::string &s) {
            write(ec, s.c_str(), s.size());
        }
        void put(std::error_code &ec, char c) {
            write(ec, &c, 1);
        }

        // Attempt to flush the write buffer
        virtual void async_flush_write_buffer(std::error_code &ec) override {
            write(ec, nullptr, 0);
        }

        using socket::connect_sync; // Import so overloads are available, see https://stackoverflow.com/questions/1628768/why-does-an-overridden-function-in-the-derived-class-hide-other-overloads-of-the?rq=1
        virtual void connect_sync(std::error_code &ec, socket_address remote) override {
            if (!is_null() && !is_bound())
                throw std::logic_error("Socket can only be connected when null or bound to a local socket");

            direct_bind(ec, remote, true);
        }

        using socket::bind; // Import so overloads are available, see https://stackoverflow.com/questions/1628768/why-does-an-overridden-function-in-the-derived-class-hide-other-overloads-of-the?rq=1
        virtual void bind(std::error_code &ec, socket_address local) override {
            if (!is_null())
                throw std::logic_error("Socket can only be bound when null");

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
            if (!is_connected() && !is_bound())
                throw std::logic_error("Socket can only be read from if connected or bound to an address");

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
            if (!is_connected() && !is_bound())
                throw std::logic_error("Socket can only be written to if connected or bound to an address");

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
        constexpr static const size_t READ_BUFFER_SIZE = 4096;

    public:
        virtual ~datagram_socket() {}

        virtual socket_type type() const noexcept final { return socket_type::datagram; }

        void set_read_limit(size_t packets) noexcept { read_buffer.set_max_size(packets); }

        socket_datagram read_datagram(std::error_code &ec) {
            socket_datagram result;

            ec.clear();

            read_buffer.read(1, [&](const socket_datagram *datagrams, size_t) {
                result = datagrams[0];
                return 1;
            });

            if (!result.is_valid())
                result = direct_read(ec);

            return result;
        }

        virtual void async_fill_read_buffer(std::error_code &ec) override {
            if (is_blocking()) {
                ec.clear();
                return;
            }

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
            async_flush_write_buffer(ec);

            if (!ec) { // No errors sending buffered packets, try new packet
                direct_write(ec, datagram.c_str(), datagram.size());
                if (!ec)
                    return;
            }

            if (queue_on_error || impl::socket_would_block(ec)) {
                write_buffer.write(socket_datagram(std::move(datagram), socket_address{}));
            }

            if (impl::socket_would_block(ec))
                ec.clear();
        }

        void write_datagram(std::error_code &ec, const socket_address &remote, std::string datagram, bool queue_on_error = true) {
            async_flush_write_buffer(ec);

            if (!ec) { // No errors sending buffered packets, try new packet
                direct_write_to(ec, datagram.c_str(), datagram.size(), remote);
                if (!ec)
                    return;
            }

            if (queue_on_error || impl::socket_would_block(ec))
                write_buffer.write(socket_datagram(std::move(datagram), remote));

            if (impl::socket_would_block(ec))
                ec.clear();
        }

        virtual void async_flush_write_buffer(std::error_code &ec) override {
            ec.clear();

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

        using socket::connect_sync; // Import so overloads are available, see https://stackoverflow.com/questions/1628768/why-does-an-overridden-function-in-the-derived-class-hide-other-overloads-of-the?rq=1
        virtual void connect_sync(std::error_code &ec, socket_address remote) override {
            if (!is_null() && !is_bound())
                throw std::logic_error("Socket can only be connected when null or bound to a local socket");

            direct_bind(ec, remote, true);
        }

        using socket::bind; // Import so overloads are available, see https://stackoverflow.com/questions/1628768/why-does-an-overridden-function-in-the-derived-class-hide-other-overloads-of-the?rq=1
        virtual void bind(std::error_code &ec, socket_address local) override {
            if (!is_null())
                throw std::logic_error("Socket can only be bound when null");

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
            socket_address remote;
            std::string data;
            const size_t pending_bytes = direct_read_pending_bytes(ec);

            if (!ec && pending_bytes <= READ_BUFFER_SIZE) {
                std::array<char, READ_BUFFER_SIZE> buf;

                const size_t bytes_read = direct_read_from(ec, buf.data(), buf.size(), remote);

                data = std::string(buf.data(), bytes_read);
            } else {
                data.resize(65535);

                data.resize(direct_read_from(ec, &data[0], data.size(), remote));
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
            if (!is_connected() && !is_bound())
                throw std::logic_error("Socket can only be written to if connected or bound to an address");

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
            if (is_connected())
                throw std::logic_error("Socket can only be written to requested address if not already connected an address");

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
    public:
        virtual ~tcp_socket() {}

        virtual socket_protocol protocol() const noexcept override { return socket_protocol::tcp; }
    };

    class udp_socket : public datagram_socket {
    public:
        virtual ~udp_socket() {}

        virtual socket_protocol protocol() const noexcept override { return socket_protocol::udp; }
    };

    class Socket;

    template<typename SocketWatcher>
    class SocketServer;

    class Socket {
        Socket(const Socket &) = delete;
        Socket(Socket &&) = delete;
        Socket &operator=(const Socket &) = delete;
        Socket &operator=(Socket &&) = delete;

        enum State {
            Unconnected,
            LookingUpHost,
            Connecting,
            Connected,
            Bound,
            Closing,
            Listening
        };

    public:
        // Handles a system error that occurred.
        //
        // If exception is thrown, the current operation is aborted
        // If return value is false then continue current operation
        // If return value is true, attempt to restart current operation
        typedef std::function<bool (Socket *, std::error_code)> ErrorFunction;

    public:
        enum Type {
            AnySocket = 0,
            StreamSocket = SOCK_STREAM,
            DatagramSocket = SOCK_DGRAM
        };

        enum Protocol {
            AnyProtocol = 0,
            TCPProtocol = IPPROTO_TCP,
            UDPProtocol = IPPROTO_UDP
        };

#if POSIX_OS
        enum Shutdown {
            ShutdownRead = SHUT_RD,
            ShutdownWrite = SHUT_WR,
            ShutdownReadWrite = SHUT_RDWR
        };
#elif WINDOWS_OS
        enum Shutdown {
            ShutdownRead = SD_RECEIVE,
            ShutdownWrite = SD_SEND,
            ShutdownReadWrite = SD_BOTH
        };
#else
# error Platform not supported
#endif

        // Reimplement in client to override behavior of address resolution
        virtual Type type() const {return AnySocket;}
        virtual Protocol protocol() const {return AnyProtocol;}

#if 0
        // Socket functionality
        // Synchronously binds to specified address and port
        Socket &bind(socket_address address) {
            if (!is_unconnected())
                throw std::logic_error("Socket was already connected or bound to an address");

            bind(address, false);

            return *this;
        }

        // Synchronously connects to specified address and port
        Socket &connect(socket_address address) {
            if (!is_unconnected() && !is_bound())
                throw std::logic_error("connect() must be called on an unconnected or bound socket");
            else if (!is_blocking())
                throw std::logic_error("connect() must only be used on blocking sockets");

            bind(address, true);

            return *this;
        }

        State state() const noexcept {return status;}
        bool is_unconnected() const noexcept {return status == Unconnected;}
        bool is_looking_up_host() const noexcept {return status == LookingUpHost;}
        bool is_connecting() const noexcept {return status == Connecting;}
        bool is_connected() const noexcept {return status == Connected;}
        bool is_bound() const noexcept {return status == Bound;}
        bool is_closing() const noexcept {return status == Closing;}
        bool is_listening() const noexcept {return status == Listening;}

    private:
        // Performs a synchronous (blocking) bind or connect to the given address
        // If address_is_remote is true, a connect() is performed
        // If address_is_remote is false, a bind() is performed
        void bind(socket_address address,
                  bool address_is_remote) {
            struct addrinfo hints;
            struct addrinfo *addresses = nullptr, *ptr = nullptr;

            status = LookingUpHost;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = address.type();
            hints.ai_socktype = type();
            hints.ai_protocol = protocol();
            hints.ai_flags = AI_PASSIVE;

            do {
                const int err = ::getaddrinfo(!address.is_unspecified()? address.to_string(false).c_str(): NULL,
                                        address.port()? std::to_string(address.port()).c_str(): NULL,
                                        &hints,
                                        &addresses);

#if POSIX_OS
                if (err == EAI_SYSTEM) {
                    if (handle_error(std::error_code(errno, socket_category())))
                        continue;
                } else
#endif
                /* else (if POSIX_OS) */ if (err) {
                    if (handle_error(std::error_code(err, getaddrinfo_error_category())))
                        continue;
                }
            } while (false);

            status = Connecting;

            int err = 0;
            do {
                try {
                    for (ptr = addresses; ptr; ptr = ptr->ai_next) {
                        const int yes = 1;              // setsockopt option
                        bool is_broadcast = false;      // is address a broadcast address

                        switch (ptr->ai_family) {
                            default: continue;
                            case IPAddressV4:
                                is_broadcast = socket_address(*reinterpret_cast<struct sockaddr_in *>(ptr->ai_addr)).is_broadcast();
                                break;
                            case IPAddressV6: break;
                        }

                        // Create new socket descriptor
                        sock = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                        if (sock == invalid_socket) {
                            err = socket_error();
                            continue;
                        }

                        // If a broadcast address, try to set the socket option
                        // It really doesn't matter if this call fails as it's just to set permissions
                        // connect() or bind() will give the real error
                        if (is_broadcast)
                            ::setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&yes), sizeof(yes));

                        if (address_is_remote) {

                            // connect() if remote address

                            if (::connect(sock, reinterpret_cast<struct sockaddr *>(ptr->ai_addr), static_cast<socklen_t>(ptr->ai_addrlen)) < 0) {
                                err = socket_error();
                                close_socket(sock);
                                sock = invalid_socket;
                                continue;
                            }
                        } else {

                            // bind() if local address

                            const int yes = 1;
                            if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes)) < 0 ||
                                ::bind(sock, reinterpret_cast<struct sockaddr *>(ptr->ai_addr), static_cast<socklen_t>(ptr->ai_addrlen)) < 0) {
                                err = socket_error();
                                close_socket(sock);
                                sock = invalid_socket;
                                continue;
                            }
                        }

                        break; // Successful, no errors; breaks address search loop, leaving ptr non-null
                    }
                } catch (...) {
                    freeaddrinfo(addresses);
                    throw;
                }

                if (ptr != nullptr) { // Successful connection was made only if ptr is nonnull
                    status = address_is_remote? Connected: Bound;
                    break; // Break error handling loop to finish up
                }

                status = Unconnected;
            } while (handle_error(err? std::error_code(err, std::system_category()): std::make_error_code(std::errc::address_not_available)));

            freeaddrinfo(addresses);

            // Prior to this call, the blocking mode could have been set but there was no descriptor to set it on
            // This needs to be done now
            set_blocking(is_blocking());
        }
#endif

    protected:
        int sock;
        State status;
        bool nonblocking;

        std::deque<io_buffer<char>> wbuffer;
        io_buffer<char> rbuffer;
    };

#if 0
    class UDPSocket : public Socket {
    public:
        UDPSocket() {}
        virtual ~UDPSocket() {}

        // Returns a size in bytes that is at least large enough for the next packet to be fully contained
        // On Linux, this returns the next packet size
        // On other platforms, this may return the cumulative size of all pending packets
        size_t pending_datagram_size() {
            // See https://stackoverflow.com/questions/9278189/how-do-i-get-amount-of-queued-data-for-udp-socket/9296481#9296481
            // See https://stackoverflow.com/questions/16995766/what-does-fionread-of-udp-datagram-sockets-return
            // This ioctl call will return the next packet size on Linux,
            // and the full size of data to be read (possibly multiple packets)
            // on other POSIX platforms

            return bytes_available();
        }

        // Reads a datagram from the socket and places the content (up to max bytes) in data
        // The address and port of the sender are placed in address and port, respectively, if provided
        //
        // Returns the size of the datagram that was just read
        //
        // For nonblocking sockets, there is no way to differentiate between "no packet available" and a zero-length packet
        size_t read_datagram(char *data, size_t max, socket_address *address = nullptr) {
            if (!is_bound() && !is_connected())
                throw std::logic_error("Socket can only use read_datagram() if bound or connected to an address");

            struct sockaddr_storage addr;
            socklen_t addrlen;

            while (true) {
                addrlen = sizeof(addr);
                const int to_read = max < INT_MAX? static_cast<int>(max): INT_MAX;
                const int read = ::recvfrom(sock, data, to_read, 0, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
                if (read < 0) {
                    const int err = socket_error();
                    if (socket_would_block(err) || !handle_error(std::error_code(err, std::system_category())))
                        return 0;
                    continue;
                }

                if (address)
                    *address = socket_address(reinterpret_cast<const struct sockaddr *>(&addr));

                return read;
            }
        }

        // Reads a datagram from the socket and sends the packet to predicate as
        // `void (const char *data, size_t len, socket_address address)`
        // with the address and port of the sender included
        //
        // Returns the size of the datagram that was just read
        //
        // For nonblocking sockets, the predicate is not called if the read would block
        template<typename Predicate>
        size_t read_datagram(Predicate p) {
            char buffer[0x1000];
            const size_t pending = pending_datagram_size();

            struct sockaddr_storage addr;
            socklen_t addrlen;

            if (pending > sizeof(buffer)) {
                std::string packet(pending, '\0');

                while (true) {
                    const int read = ::recvfrom(sock, &packet[0], packet.size(), 0, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
                    if (read < 0) {
                        const int err = socket_error();
                        if (socket_would_block(err) || !handle_error(std::error_code(err, std::system_category())))
                            return 0;
                        continue;
                    }

                    socket_address address(reinterpret_cast<struct sockaddr *>(&addr));
                    p(packet.c_str(), read, address);

                    return read;
                }
            } else {
                while (true) {
                    const int read = ::recvfrom(sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
                    if (read < 0) {
                        const int err = socket_error();
                        if (socket_would_block(err) || !handle_error(std::error_code(err, std::system_category())))
                            return 0;
                        continue;
                    }

                    socket_address address(reinterpret_cast<struct sockaddr *>(&addr));
                    p(buffer, read, address);

                    return read;
                }
            }
        }

        std::string read_datagram(socket_address *address = nullptr) {
            std::string datagram(pending_datagram_size(), '\0');
            datagram.resize(read_datagram(&datagram[0], datagram.size(), address));
            return datagram;
        }

        // Writes a datagram to the socket
        // The address and port of the recipient must be specified
        //
        // Returns the number of bytes written
        //
        // For nonblocking sockets, there is no way to differentiate between "failed to send" and a zero-length packet
        size_t write_datagram(socket_address address, const char *data, size_t len) {
            if (!is_bound() && !is_connected())
                throw std::logic_error("Socket can only use write_datagram() if bound or connected to an address");

            const int yes = 1;
            struct sockaddr_storage addr = address.native();

            // Enable broadcast if a broadcast address
            if (address.is_broadcast() &&
                             ::setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&yes), sizeof(yes)) < 0)
                handle_error(std::error_code(socket_error(), std::system_category()));

            // Send packet to socket
            while (true) {
                const int to_write = len < INT_MAX? static_cast<int>(len): INT_MAX;
                const int written = ::sendto(sock, data, to_write, 0,
                                             reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
                if (written < 0) {
                    const int err = socket_error();
                    if (socket_would_block(err) || !handle_error(std::error_code(err, std::system_category())))
                        return 0;
                    continue;
                }

                return written;
            }
        }
        size_t write_datagram(socket_address address, const char *data) {
            return write_datagram(address, data, strlen(data));
        }
        size_t write_datagram(socket_address address, const std::string &str) {
            return write_datagram(address, str.c_str(), str.length());
        }

        virtual Type type() const {return DatagramSocket;}
        virtual Protocol protocol() const {return UDPProtocol;}
    };
#endif
}

#endif // SKATE_SOCKET_H
