#ifndef SKATE_SOCKET_H
#define SKATE_SOCKET_H

#include "address.h"
#include "../io/device.h"

#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <cstddef>
#include <climits>

#if POSIX_OS
# include <ifaddrs.h> // For getting interfaces
#endif

// See https://beej.us/guide/bgnet/html

namespace Skate {
    class Socket;

    class SocketError : public std::runtime_error
    {
        Socket *sock;
        int error;

    public:
        SocketError(Socket *sock, int system_error)
            : std::runtime_error(system_error_string(system_error).to_utf8())
            , sock(sock)
            , error(system_error)
        {}

        Socket *socket() const noexcept {return sock;}
        int native_error() const noexcept {return error;}
    };

    class Socket : public IODevice {
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

    protected:
        static constexpr SocketDescriptor invalid_socket = -1;

#if POSIX_OS
        static inline int close_socket(SocketDescriptor s) {return ::close(s);}
        static inline int socket_error() {return errno;}
        static constexpr int no_address = EADDRNOTAVAIL;
#elif WINDOWS_OS
        static inline int close_socket(SocketDescriptor s) {return ::closesocket(s);}
        static inline int socket_error() {return ::WSAGetLastError();}
        static constexpr int no_address = ERROR_HOST_UNREACHABLE;
#endif

        std::function<void (Socket *, int)> _on_error;
        void handle_error(int system_error) {
            if (system_error == 0)
                return;

            if (_on_error)
                _on_error(this, system_error);
            else
                throw SocketError(this, system_error);
        }

        Socket(SocketDescriptor sock)
            : sock(sock)
            , status(sock == invalid_socket? Unconnected: Connected)
        {}

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
#endif

        // Initialization and destruction
        Socket()
            : sock(invalid_socket)
            , status(Unconnected)
            , nonblocking(false)
        {}
        virtual ~Socket() {
            try {
                disconnect();
            } catch (const SocketError &) {}
        }

        // Error handling callback
        template<typename Predicate>
        void on_error(Predicate p) {_on_error = p;}

        // Reimplement in client to override behavior of address resolution
        virtual Type type() const {return AnySocket;}
        virtual Protocol protocol() const {return AnyProtocol;}

        // Socket functionality
        // Synchronously binds to specified address and port
        Socket &bind(SocketAddress address, uint16_t port) {
            if (!is_unconnected())
                throw std::logic_error("Socket was already connected or bound to an address");

            handle_error(try_bind(type(), protocol(), address, port, false));

            return *this;
        }
        // Synchronously connects to specified address and port
        Socket &connect(SocketAddress address, uint16_t port) {
            if (!is_unconnected())
                throw std::logic_error("Socket was already connected or bound to an address");

            handle_error(try_bind(type(), protocol(), address, port, true));

            return *this;
        }

        // Starts socket listening for connections
        Socket &listen(int backlog = SOMAXCONN) {
            if (!is_bound())
                throw std::logic_error("Socket can only use listen() if bound to an address");

            handle_error(::listen(sock, backlog));

            return *this;
        }

        // Returns remote address information (only if connected)
        // port is in native byte order, and is indeterminate if an error occurs
        SocketAddress remote_address(uint16_t *port = nullptr) {
            if (!is_connected())
                throw std::logic_error("Socket can only use remote_address()/remote_port() if connected to a remote server");

            struct sockaddr_storage addr;
            socklen_t addrlen = sizeof(addr);

            const int err = ::getpeername(sock, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
            if (err) {
                handle_error(err);
                return {};
            }

            if (port != nullptr) {
                switch (addr.ss_family) {
                    default: break;
                    case SocketAddress::IPAddressV4: *port = ntohs(reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port); break;
                    case SocketAddress::IPAddressV6: *port = ntohs(reinterpret_cast<struct sockaddr_in6 *>(&addr)->sin6_port); break;
                }
            }

            return SocketAddress{&addr};
        }
        uint16_t remote_port() {
            uint16_t port = 0;
            remote_address(&port);
            return port;
        }

        // Returns local address information (only if connected or bound)
        // port is in native byte order, and is indeterminate if an error occurs
        SocketAddress local_address(uint16_t *port = nullptr) {
            if (!is_connected())
                throw std::logic_error("Socket can only use local_address()/local_port() if connected to a remote server");

            struct sockaddr_storage addr;
            socklen_t addrlen = sizeof(addr);

            const int err = ::getsockname(sock, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
            if (err) {
                handle_error(err);
                return {};
            }

            if (port != nullptr) {
                switch (addr.ss_family) {
                    default: break;
                    case SocketAddress::IPAddressV4: *port = ntohs(reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port); break;
                    case SocketAddress::IPAddressV6: *port = ntohs(reinterpret_cast<struct sockaddr_in6 *>(&addr)->sin6_port); break;
                }
            }

            return SocketAddress{&addr};
        }
        uint16_t local_port() {
            uint16_t port = 0;
            local_address(&port);
            return port;
        }

        // Runs message loop listening and accepting connections
        template<typename Handler>
        void run() {

        }

        // Synchronously disconnects connection
        Socket &disconnect() {
            SocketDescriptor temp = sock;
            sock = invalid_socket;
            status = Unconnected;

            if (temp != invalid_socket)
                handle_error(close_socket(temp));

            return *this;
        }

        Socket &shutdown(Shutdown type = ShutdownReadWrite) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use shutdown() if connected or bound to an address");

            handle_error(::shutdown(sock, type));

            return *this;
        }

        // Synchronously writes all data to socket
        Socket &write(const char *data, size_t len) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use write()/put() if connected or bound to an address");
            else if (!is_blocking())
                throw std::logic_error("Socket can only use write()/put() if blocking");

            while (len) {
                const int to_send = len < INT_MAX? static_cast<int>(len): INT_MAX;
                const int sent = ::send(sock, data, to_send, 0);
                if (sent < 0) {
                    handle_error(socket_error());
                    break;
                }

                data += sent;
                len -= sent;
            }

            return *this;
        }
        Socket &write(const char *data) {return write(data, strlen(data));}
        Socket &write(const std::string &data) {return write(&data[0], data.length());}
        Socket &put(unsigned char c) {return write(reinterpret_cast<const char *>(&c), 1);}

        // Synchronously reads data from socket
        // Returns number of bytes read
        size_t read(char *data, size_t max) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use read() if connected or bound to an address");
            else if (!is_blocking())
                throw std::logic_error("Socket can only use read() if blocking");

            const size_t original_max = max;
            while (max) {
                const int to_read = max < INT_MAX? static_cast<int>(max): INT_MAX;
                const int read = ::recv(sock, data, to_read, 0);
                if (read == 0) {
                    return original_max - max;
                } else if (read < 0) {
                    handle_error(socket_error());
                    return original_max - max;
                }

                data += read;
                max -= read;
            }

            return original_max;
        }
        template<typename Predicate>
        void read(size_t max, Predicate predicate) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use read() if connected or bound to an address");
            else if (!is_blocking())
                throw std::logic_error("Socket can only use read() if blocking");

            char buffer[0x1000];
            while (max) {
                const int to_read = max < sizeof(buffer)? static_cast<int>(max): sizeof(buffer);
                const int read = ::recv(sock, buffer, to_read, 0);
                if (read == 0) {
                    return;
                } else if (read < 0) {
                    handle_error(socket_error());
                    return;
                }

                predicate(static_cast<const char *>(buffer), read);
                max -= read;
            }
        }
        template<typename Predicate>
        void read_all(Predicate predicate) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use read_all() if connected or bound to an address");
            else if (!is_blocking())
                throw std::logic_error("Socket can only use read_all() if blocking");

            char buffer[0x1000];
            while (true) {
                const int read = ::recv(sock, buffer, sizeof(buffer), 0);
                if (read == 0) {
                    return;
                } else if (read < 0) {
                    handle_error(socket_error());
                    return;
                }

                predicate(static_cast<const char *>(buffer), read);
            }
        }

        std::string read(size_t max) {
            std::string result;

            read(max, [&result](const char *data, size_t len) {
                result.append(data, len);
            });

            return result;
        }
        std::string read_all() {
            std::string result;

            read_all([&result](const char *data, size_t len) {
                result.append(data, len);
            });

            return result;
        }

        bool is_blocking() const noexcept {
            if (sock == invalid_socket)
                return false;

            return !nonblocking;
        }
        void set_blocking(bool b) {
#if POSIX_OS
            const int flags = ::fcntl(sock, F_GETFL);
            if (flags < 0 ||
                ::fcntl(sock, F_SETFL, b? (flags & ~O_NONBLOCK): (flags | O_NONBLOCK)) < 0)
                handle_error(socket_error());
#elif WINDOWS_OS
            u_long opt = !b;
            if (::ioctlsocket(sock, FIONBIO, &opt) < 0)
                handle_error(socket_error());
#endif

            nonblocking = !b;
        }

        State state() const noexcept {return status;}
        bool is_unconnected() const noexcept {return status == Unconnected;}
        bool is_looking_up_host() const noexcept {return status == LookingUpHost;}
        bool is_connecting() const noexcept {return status == Connecting;}
        bool is_connected() const noexcept {return status == Connected;}
        bool is_bound() const noexcept {return status == Bound;}
        bool is_closing() const noexcept {return status == Closing;}
        bool is_listening() const noexcept {return status == Listening;}

        static std::vector<SocketAddress> interfaces(SocketAddress::Type type = SocketAddress::IPAddressUnspecified, bool include_loopback = false) {
            struct ifaddrs *addresses = nullptr, *ptr = nullptr;
            std::vector<SocketAddress> result;

            const int err = ::getifaddrs(&addresses);
            if (err != 0)
                throw SocketError(nullptr, err);

            try {
                for (ptr = addresses; ptr; ptr = ptr->ifa_next) {
                    SocketAddress address;

                    switch (ptr->ifa_addr->sa_family) {
                        default: break;
                        case SocketAddress::IPAddressV4:
                            if (type == SocketAddress::IPAddressUnspecified || type == SocketAddress::IPAddressV4)
                                address = SocketAddress{reinterpret_cast<struct sockaddr_in *>(ptr->ifa_addr)};
                            break;
                        case SocketAddress::IPAddressV6:
                            if (type == SocketAddress::IPAddressUnspecified || type == SocketAddress::IPAddressV6)
                                address = SocketAddress{reinterpret_cast<struct sockaddr_in6 *>(ptr->ifa_addr)};
                            break;
                    }

                    if (address && (!address.is_loopback() || include_loopback))
                        result.push_back(std::move(address));
                }
            } catch (...) {
                freeifaddrs(addresses);
                throw;
            }

            freeifaddrs(addresses);

            return result;
        }

    private:
        int try_bind(Type socket_type,
                     Protocol protocol_type,
                     SocketAddress address,
                     uint16_t port,
                     bool address_is_remote) {
            struct addrinfo hints;
            struct addrinfo *addresses = nullptr, *ptr = nullptr;

            sock = invalid_socket; // Initialize socket to invalid before attempting any system calls so it will be invalid if an error occurs
            status = LookingUpHost;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = address.type();
            hints.ai_socktype = socket_type;
            hints.ai_protocol = protocol_type;
            hints.ai_flags = AI_PASSIVE;

            int err = ::getaddrinfo(address? address.to_string().c_str(): NULL,
                                    port? std::to_string(port).c_str(): NULL,
                                    &hints,
                                    &addresses);
            if (err)
                return err;

            status = Connecting;

            try {
                for (ptr = addresses; ptr; ptr = ptr->ai_next) {
                    switch (ptr->ai_family) {
                        default: continue;
                        case SocketAddress::IPAddressV4: /* fallthrough */
                        case SocketAddress::IPAddressV6: break;
                    }

                    sock = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                    if (sock == invalid_socket) {
                        err = socket_error();
                        return err;
                    }

                    if (address_is_remote) { // connect() here if remote address
                        if (::connect(sock, reinterpret_cast<struct sockaddr *>(ptr->ai_addr), static_cast<socklen_t>(ptr->ai_addrlen)) < 0) {
                            err = socket_error();
                            close_socket(sock);
                            sock = invalid_socket;
                            continue;
                        }
                    } else { // bind() here if local address
                        const int yes = 1;
                        if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes)) < 0 ||
                            ::bind(sock, reinterpret_cast<struct sockaddr *>(ptr->ai_addr), static_cast<socklen_t>(ptr->ai_addrlen)) < 0) {
                            err = socket_error();
                            close_socket(sock);
                            sock = invalid_socket;
                            continue;
                        }
                    }

                    break;
                }
            } catch (...) {
                freeaddrinfo(addresses);
                throw;
            }

            freeaddrinfo(addresses);

            if (ptr != nullptr) // Only non-NULL if valid address was bound or connected
                status = address_is_remote? Connected: Bound;
            else {
                status = Unconnected;
                if (!err)
                    err = no_address;
            }

            return err;
        }

    protected:
        SocketDescriptor sock;
        State status;
        bool nonblocking;
    };

    class UDPSocket : public Socket {
    public:
        UDPSocket() {}
        virtual ~UDPSocket() {}

        virtual Type type() const {return DatagramSocket;}
        virtual Protocol protocol() const {return UDPProtocol;}
    };

    class TCPSocket : public Socket {
    public:
        TCPSocket() {}
        virtual ~TCPSocket() {}

        virtual Type type() const {return StreamSocket;}
        virtual Protocol protocol() const {return TCPProtocol;}
    };

#if WINDOWS_OS
    class EventList;

    class Event {
        friend class EventList;

        std::shared_ptr<void> event;

    public:
        Event(std::nullptr_t) {}
        Event(bool manual_reset = false, bool signalled = false)
            : event(std::shared_ptr<void>(::CreateEvent(NULL, manual_reset, signalled, NULL),
                                          [](HANDLE event) {
                                              if (event)
                                                  ::CloseHandle(event);
                                          }))
        {
            if (!event)
                throw std::runtime_error(system_error_string(GetLastError()).to_utf8());
        }

        bool is_null() const {return event.get() == nullptr;}
        void signal() {
            if (event && !::SetEvent(event.get()))
                throw std::runtime_error(system_error_string(GetLastError()).to_utf8());
        }
        void reset() {
            if (event && !::ResetEvent(event.get()))
                throw std::runtime_error(system_error_string(GetLastError()).to_utf8());
        }
        void set_state(bool signalled) {
            if (signalled)
                signal();
            else
                reset();
        }

        // Waits for event to be signalled. An exception is thrown if an error occurs.
        void wait() {
            if (::WaitForSingleObject(event.get(), INFINITE) == WAIT_FAILED)
                throw std::runtime_error(system_error_string(GetLastError()).to_utf8());
        }
        // Waits for event to be signalled. If true, event was signalled. If false, timeout occurred. An exception is thrown if an error occurs.
        bool wait(std::chrono::milliseconds timeout) {
            if (timeout.count() >= INFINITE)
                throw std::runtime_error("wait() called with invalid timeout value");

            const DWORD result = ::WaitForSingleObject(event.get(), static_cast<DWORD>(timeout.count()));

            switch (result) {
                case WAIT_FAILED:
                    throw std::runtime_error(system_error_string(GetLastError()).to_utf8());
                case WAIT_OBJECT_0:
                    return true;
                default:
                    return false;
            }
        }

        bool operator==(const Event &other) const {
            return event == other.event;
        }
        bool operator!=(const Event &other) const {
            return !operator==(other);
        }
        operator bool() const {
            return !is_null();
        }
    };

    class EventList {
        std::vector<Event> events;
        mutable std::vector<HANDLE> cache;

        void rebuild_cache() const {
            if (cache.empty()) {
                for (const Event &ev: events) {
                    cache.push_back(ev.event.get());
                }
            }
        }

    public:
        EventList() {}

        size_t size() const noexcept {return events.size();}
        EventList &add(Event ev) {
            if (std::find(events.begin(), events.end(), ev) == events.end()) {
                if (events.size() == MAXIMUM_WAIT_OBJECTS)
                    throw std::runtime_error("Attempted to wait on more than " + std::to_string(MAXIMUM_WAIT_OBJECTS) + " events with EventList");

                events.push_back(ev);
                cache.clear();
            }

            return *this;
        }
        EventList &operator<<(const Event &ev) {return add(ev);}
        EventList &remove(Event ev) {
            const auto it = std::find(events.begin(), events.end(), ev);
            if (it != events.end()) {
                events.erase(it);
                cache.clear();
            }

            return *this;
        }

        Event wait() const {
            rebuild_cache();

            const DWORD result = ::WaitForMultipleObjects(static_cast<DWORD>(cache.size()),
                                                          cache.data(),
                                                          FALSE,
                                                          INFINITE);

            switch (result) {
                case WAIT_FAILED:
                    throw std::runtime_error(system_error_string(GetLastError()).to_utf8());
                default:
                    if (result - WAIT_OBJECT_0 < MAXIMUM_WAIT_OBJECTS)
                        return events[result - WAIT_OBJECT_0];
                    else
                        return events[result - WAIT_ABANDONED_0];
            }
        }
        Event wait(std::chrono::milliseconds timeout) const {
            rebuild_cache();

            if (timeout.count() >= INFINITE)
                throw std::runtime_error("wait() called with invalid timeout value");

            const DWORD result = ::WaitForMultipleObjects(static_cast<DWORD>(cache.size()),
                                                          cache.data(),
                                                          FALSE,
                                                          static_cast<DWORD>(timeout.count()));

            switch (result) {
                case WAIT_FAILED:
                    throw std::runtime_error(system_error_string(GetLastError()).to_utf8());
                case WAIT_TIMEOUT:
                    return {nullptr};
                default:
                    if (result - WAIT_OBJECT_0 < MAXIMUM_WAIT_OBJECTS)
                        return events[result - WAIT_OBJECT_0];
                    else
                        return events[result - WAIT_ABANDONED_0];
            }
        }
    };

#endif
}

#endif // SKATE_SOCKET_H
