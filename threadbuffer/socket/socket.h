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

#include <mutex>

#if POSIX_OS
# include <ifaddrs.h> // For getting interfaces
#elif WINDOWS_OS
# include <iphlpapi.h> // For getting interfaces
#
# if MSVC_COMPILER
#  pragma comment(lib, "iphlpapi.lib")
# endif
#endif

// See https://beej.us/guide/bgnet/html

namespace Skate {
    class Socket;

    class SocketError : public std::runtime_error
    {
        Socket *sock;
        int error;

    public:
        SocketError(Socket *sock, int system_error, const char *description = nullptr)
            : std::runtime_error(description && strlen(description)?
                                     std::string(description):
                                     system_error_string(system_error).to_utf8())
            , sock(sock)
            , error(system_error)
        {}

        Socket *socket() const noexcept {return sock;}
        int native_error() const noexcept {return error;}
    };

    template<typename SocketWatcher>
    class SocketServer;

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

    public:
        typedef std::function<bool (Socket *, int, const char *)> ErrorFunction;

    protected:
        template<typename SocketWatcher>
        friend class SocketServer;

#if POSIX_OS
        static inline int close_socket(SocketDescriptor s) {return ::close(s);}
        static inline bool socket_would_block(int system_error) {
            return system_error == EAGAIN || system_error == EWOULDBLOCK;
        }
        static inline int socket_error() {return errno;}
        static constexpr int no_address = EADDRNOTAVAIL;
#elif WINDOWS_OS
        static inline int close_socket(SocketDescriptor s) {return ::closesocket(s);}
        static inline int socket_error() {return ::WSAGetLastError();}
        static constexpr int no_address = ERROR_HOST_UNREACHABLE;
#endif

        ErrorFunction _on_error;

        // Handles a system error that occurred.
        //
        // If description is non-null, it provides a user-readable description of what error occurred.
        //
        // If return value is false (or never returns) then abort current operation.
        // If return value is true, attempt to continue current operation.
        bool handle_error(int system_error, const char *description = nullptr) {
            if (system_error == 0)
                return false;

            if (_on_error)
                return _on_error(this, system_error, description);
            else {
                default_error_handler(this, system_error, description);
                return false; // Never reached as default handler throws
            }
        }

        Socket(SocketDescriptor sock)
            : sock(sock)
            , status(sock == invalid_socket? Unconnected: Connected)
            , nonblocking(false)
        {}

    public:
        static constexpr SocketDescriptor invalid_socket = -1;

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

        // Default error handler (can be used in an external error handler if necessary)
        static void default_error_handler(Socket *socket, int system_error, const char *description = nullptr) {
            throw SocketError(socket, system_error, description);
        }

        // Returns the native socket descriptor
        SocketDescriptor native() const {return sock;}

        // Error handling callback
        void on_error(ErrorFunction fn) {_on_error = fn;}

        // Reimplement in client to override behavior of address resolution
        virtual Type type() const {return AnySocket;}
        virtual Protocol protocol() const {return AnyProtocol;}

        // Socket functionality
        // Synchronously binds to specified address and port
        Socket &bind(SocketAddress address, uint16_t port) {
            if (!is_unconnected())
                throw std::logic_error("Socket was already connected or bound to an address");

            bind(type(), protocol(), address, port, false);

            return *this;
        }
        // Synchronously connects to specified address and port
        Socket &connect(SocketAddress address, uint16_t port) {
            if (!is_unconnected())
                throw std::logic_error("Socket was already connected or bound to an address");
            else if (!is_blocking())
                throw std::logic_error("connect() must only be used on blocking sockets");

            bind(type(), protocol(), address, port, true);

            return *this;
        }

        // Starts socket listening for connections
        Socket &listen(int backlog = SOMAXCONN) {
            if (!is_bound())
                throw std::logic_error("Socket can only use listen() if bound to an address");

            const int err = ::listen(sock, backlog);
            if (err)
                handle_error(err);
            else
                status = Listening;

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

        // Synchronously disconnects connection
        Socket &disconnect() {
            SocketDescriptor temp = sock;
            sock = invalid_socket;
            status = Unconnected;

            if (temp != invalid_socket)
                handle_error(close_socket(temp));

            return *this;
        }

        // Shuts down either the read part of the socket, the write part, or both
        Socket &shutdown(Shutdown type = ShutdownReadWrite) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use shutdown() if connected or bound to an address");

            handle_error(::shutdown(sock, type));

            return *this;
        }

        // Synchronously writes all data to socket
        // Returns the number of bytes successfully written
        //
        // For nonblocking sockets, this will only write data until it will not block
        size_t write(const char *data, size_t len) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use write()/put() if connected or bound to an address");

            const size_t original_size = len;
            while (len) {
                const int to_send = len < INT_MAX? static_cast<int>(len): INT_MAX;
                const int sent = ::send(sock, data, to_send, 0);
                if (sent < 0) {
                    const int err = socket_error();
                    if (socket_would_block(err) || !handle_error(err))
                        return original_size - len;
                    continue;
                }

                data += sent;
                len -= sent;
            }

            return original_size;
        }
        Socket &write(const char *data) {
            if (!is_blocking())
                throw std::logic_error("write() variants must be used only on blocking sockets");

            write(data, strlen(data));
            return *this;
        }
        Socket &write(const std::string &data) {
            if (!is_blocking())
                throw std::logic_error("write() variants must be used only on blocking sockets");

            write(&data[0], data.length());
            return *this;
        }
        Socket &put(unsigned char c) {
            if (!is_blocking())
                throw std::logic_error("put() must be used only on blocking sockets");

            write(reinterpret_cast<const char *>(&c), 1);
            return *this;
        }

        // Synchronously reads data from socket
        // Returns number of bytes read
        //
        // For nonblocking sockets, this will only read all the data currently available for reading in the socket, up to max
        size_t read(char *data, size_t max) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use read() if connected or bound to an address");

            const size_t original_max = max;
            while (max) {
                const int to_read = max < INT_MAX? static_cast<int>(max): INT_MAX;
                const int read = ::recv(sock, data, to_read, 0);
                if (read == 0) {
                    return original_max - max;
                } else if (read < 0) {
                    const int err = socket_error();
                    if (socket_would_block(err) || handle_error(err))
                        return original_max - max;
                    continue;
                }

                data += read;
                max -= read;
            }

            return original_max;
        }
        // Synchronously reads up to max bytes from the socket and sends them to predicate as
        // void (const char *data, size_t len);
        // The predicate may be called several times with new data, it should be considered an append function
        //
        // For nonblocking sockets, this will only read data currently available for reading in the socket, up to max bytes
        template<typename Predicate>
        void read(uint64_t max, Predicate predicate) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use read() if connected or bound to an address");

            char buffer[0x1000];
            while (max) {
                const int to_read = max < sizeof(buffer)? static_cast<int>(max): sizeof(buffer);
                const int read = ::recv(sock, buffer, to_read, 0);
                if (read == 0) {
                    return;
                } else if (read < 0) {
                    const int err = socket_error();
                    if (socket_would_block(err) || !handle_error(err))
                        return;
                    continue;
                }

                predicate(static_cast<const char *>(buffer), read);
                max -= read;
            }
        }
        // Synchronously reads all data from the socket and sends it to predicate as
        // void (const char *data, size_t len);
        // The predicate may be called several times with new data, it should be considered an append function
        //
        // For nonblocking sockets, this will only read all the data currently available for reading in the socket
        template<typename Predicate>
        void read_all(Predicate predicate) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use read_all() if connected or bound to an address");

            char buffer[0x1000];
            while (true) {
                const int read = ::recv(sock, buffer, sizeof(buffer), 0);
                if (read == 0) {
                    return;
                } else if (read < 0) {
                    const int err = socket_error();
                    if (socket_would_block(err) || !handle_error(err))
                        return;
                    continue;
                }

                predicate(static_cast<const char *>(buffer), read);
            }
        }
        void read(size_t max, std::string &str) {
            read(max, [&str](const char *data, size_t len) {
                str.append(data, len);
            });
        }
        void read(size_t max, std::ostream &os) {
            read(max, [&os](const char *data, size_t len) {
                os.write(data, len);
            });
        }
        void read(size_t max, FILE *out) {
            read(max, [out](const char *data, size_t len) {
                fwrite(data, 1, len, out);
            });
        }
        std::string read(size_t max) { std::string result; read(max, result); return result; }
        void read_all(std::string &str) {
            read_all([&str](const char *data, size_t len) {
                str.append(data, len);
            });
        }
        std::string read_all() { std::string result; read_all(result); return result; }
        void read_all(std::ostream &os) {
            read_all([&os](const char *data, size_t len) {
                os.write(data, len);
            });
        }
        void read_all(FILE *out) {
            read_all([out](const char *data, size_t len) {
                fwrite(data, 1, len, out);
            });
        }

        // Returns true if this socket is blocking (the default)
        bool is_blocking() const noexcept {return !nonblocking;}

        // Sets whether this socket is blocking or asynchronous
        void set_blocking(bool b) {
            if (sock == invalid_socket) {
                nonblocking = !b;
                return;
            }

#if POSIX_OS
            const int flags = ::fcntl(sock, F_GETFL);
            if (flags < 0 ||
                ::fcntl(sock, F_SETFL, b? (flags & ~O_NONBLOCK): (flags | O_NONBLOCK)) < 0)
                handle_error(socket_error());
            else
                nonblocking = !b;
#elif WINDOWS_OS
            u_long opt = !b;
            if (::ioctlsocket(sock, FIONBIO, &opt) < 0)
                handle_error(socket_error());
            else
                nonblocking = !b;
#endif
        }

        State state() const noexcept {return status;}
        bool is_unconnected() const noexcept {return status == Unconnected;}
        bool is_looking_up_host() const noexcept {return status == LookingUpHost;}
        bool is_connecting() const noexcept {return status == Connecting;}
        bool is_connected() const noexcept {return status == Connected;}
        bool is_bound() const noexcept {return status == Bound;}
        bool is_closing() const noexcept {return status == Closing;}
        bool is_listening() const noexcept {return status == Listening;}

        // Returns the local interface addresses for the local computer
        // Only active addresses are returned, and the desired types can be filtered with the parameters to this function
        static std::vector<SocketAddress> interfaces(SocketAddress::Type type = SocketAddress::IPAddressUnspecified, bool include_loopback = false) {
#if POSIX_OS
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
#elif WINDOWS_OS
            IP_ADAPTER_ADDRESSES *addresses = nullptr;
            ULONG addresses_buffer_size = 15000, err = 0;
            std::vector<SocketAddress> result;

            do {
                addresses = static_cast<IP_ADAPTER_ADDRESSES *>(malloc(addresses_buffer_size));

                err = ::GetAdaptersAddresses(type, 0, NULL, addresses, &addresses_buffer_size);
                if (err == ERROR_BUFFER_OVERFLOW)
                    free(addresses);
            } while (err == ERROR_BUFFER_OVERFLOW);

            try {
                if (err != ERROR_SUCCESS)
                    throw SocketError(nullptr, err); // Will rethrow in catch block

                for (auto ptr = addresses; ptr; ptr = ptr->Next) {
                    // Skip network interfaces that are not up and running
                    if (ptr->OperStatus != IfOperStatusUp)
                        continue;

                    // Skip loopback if desired
                    if (!include_loopback && ptr->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                        continue;

                    // Just look for unicast addresses
                    for (auto unicast = ptr->FirstUnicastAddress; unicast; unicast = unicast->Next) {
                        SocketAddress address;

                        switch (unicast->Address.lpSockaddr->sa_family) {
                            default:
                            case SocketAddress::IPAddressV4: address = SocketAddress(reinterpret_cast<struct sockaddr_in *>(unicast->Address.lpSockaddr)); break;
                            case SocketAddress::IPAddressV6: address = SocketAddress(reinterpret_cast<struct sockaddr_in6 *>(unicast->Address.lpSockaddr)); break;
                        }

                        if (address && (!address.is_loopback() || include_loopback))
                            result.push_back(std::move(address));
                    }
                }

                free(addresses);
            } catch (...) {
                free(addresses);
                throw;
            }

            return result;
#endif
        }

    private:
        // Performs a synchronous (blocking) bind or connect to the given address
        // If address_is_remote is true, a connect() is performed
        // If address_is_remote is false, a bind() is performed
        void bind(Type socket_type,
                  Protocol protocol_type,
                  SocketAddress address,
                  uint16_t port,
                  bool address_is_remote) {
            static std::mutex gai_strerror_mtx;
            struct addrinfo hints;
            struct addrinfo *addresses = nullptr, *ptr = nullptr;

            status = LookingUpHost;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = address.type();
            hints.ai_socktype = socket_type;
            hints.ai_protocol = protocol_type;
            hints.ai_flags = AI_PASSIVE;

            int err = 0;
            std::string err_description;

            do {
                err = ::getaddrinfo(address? address.to_string().c_str(): NULL,
                                    port? std::to_string(port).c_str(): NULL,
                                    &hints,
                                    &addresses);

                if (err == EAI_SYSTEM) {
                    err = errno;
                    err_description.clear();
                } else if (err) {
                    {
                        std::lock_guard<std::mutex> lock(gai_strerror_mtx);
                        err_description = gai_strerror(err);
                    }

                    // Map to the most similar system errors
                    switch (err) {
                        case EAI_ADDRFAMILY:                  break; // No mapping
                        case EAI_AGAIN:         err = EAGAIN; break;
                        case EAI_BADFLAGS:                    break; // Won't happen
                        case EAI_FAIL:                        break;
                        case EAI_FAMILY:                      break;
                        case EAI_MEMORY:        err = ENOMEM; break;
                        case EAI_NODATA:                      break;
                        case EAI_NONAME:                      break;
                        case EAI_SERVICE:                     break;
                        case EAI_SOCKTYPE:                    break;
                    }
                } else {
                    err_description.clear();
                }
            } while (handle_error(err, err_description.c_str()));

            status = Connecting;

            do {
                try {
                    for (ptr = addresses; ptr; ptr = ptr->ai_next) {
                        switch (ptr->ai_family) {
                            default: continue;
                            case SocketAddress::IPAddressV4: break;
                            case SocketAddress::IPAddressV6: break;
                        }

                        sock = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                        if (sock == invalid_socket) {
                            err = socket_error();
                            continue;
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

                        break; // Successful, no errors; breaks address search loop, leaving ptr non-null
                    }
                } catch (...) {
                    freeaddrinfo(addresses);
                    throw;
                }

                if (ptr != nullptr) { // Successful connection was made only if ptr is nonnull
                    status = address_is_remote? Connected: Bound;
                    break;
                }

                status = Unconnected;
            } while (handle_error(err? err: no_address));

            freeaddrinfo(addresses);

            // Prior to this call, the blocking mode could have been set but there was no descriptor to set it on
            // This needs to be done now
            set_blocking(is_blocking());
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
