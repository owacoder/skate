#ifndef SKATE_SOCKET_H
#define SKATE_SOCKET_H

#include "address.h"
#include "../io/buffer.h"

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
# include <ifaddrs.h> // For getting interfaces
# include <sys/ioctl.h>
#elif WINDOWS_OS
# include <iphlpapi.h> // For getting interfaces
#
# if MSVC_COMPILER
#  pragma comment(lib, "iphlpapi")
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
        // If description is non-null, it provides a user-readable description of what error occurred.
        //
        // If exception is thrown, the current operation is aborted
        // If return value is false then continue current operation
        // If return value is true, attempt to restart current operation
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
        static inline bool socket_would_block(int system_error) {
            return system_error == WSAEWOULDBLOCK;
        }
        static inline int socket_error() {return ::WSAGetLastError();}
        static constexpr int no_address = ERROR_HOST_UNREACHABLE;
#else
# error Platform not supported
#endif

        ErrorFunction _on_error;

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

        Socket(SocketDescriptor sock, bool nonblocking = false)
            : sock(sock)
            , status(sock == invalid_socket? Unconnected: Connected)
            , nonblocking(nonblocking)
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
#else
# error Platform not supported
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

        // Socket functionality
        // Synchronously binds to specified address and port
        Socket &bind(SocketAddress address, uint16_t port) {
            if (!is_unconnected())
                throw std::logic_error("Socket was already connected or bound to an address");

            bind(address, port, false);

            return *this;
        }
        // Synchronously connects to specified address and port
        Socket &connect(SocketAddress address, uint16_t port) {
            if (!is_unconnected() && !is_bound())
                throw std::logic_error("connect() must be called on an unconnected or bound socket");
            else if (!is_blocking())
                throw std::logic_error("connect() must only be used on blocking sockets");

            bind(address, port, true);

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

        // Synchronously write data to socket
        // Returns the number of bytes successfully written
        //
        // For stream sockets:
        //     For nonblocking sockets, this will only write data until it will not block
        //
        // For datagram sockets:
        //     This will send the data as a single datagram
        //     There is no way to differentiate between zero-length outgoing packets and a failure to send on a nonblocking socket
        //
        size_t write(const char *data, size_t len) {
            if (!is_connected() && !is_bound() && !is_listening())
                throw std::logic_error("Socket can only use write()/put() if connected or bound to an address");

            const size_t original_size = len;
            while (len) {
                const int to_send = len < INT_MAX? static_cast<int>(len): INT_MAX;
                const int sent = ::send(sock, data, to_send, MSG_NOSIGNAL);
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
        // For stream sockets:
        //     For nonblocking sockets, this will only read all the data currently available for reading in the socket, up to max
        //        If the read would block, the predicate is not called
        //
        // For datagram sockets:
        //     This will read one datagram from the socket, truncating the message if it doesn't fit in the provided buffer
        //     There is no way to differentiate between zero-length incoming packets and a failure to read on a nonblocking socket
        //
        size_t read(char *data, size_t max) {
            if (!is_connected() && !is_bound())
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

        // A simple, user-configurable storage location for TCP (or similar stream socket) read data
        InputOutputBuffer<char> &read_buffer() { return rbuffer; }

        // A simple, user-configurable storage location for UDP/TCP (or really any socket type) write data
        // Data is written in

        // Synchronously reads up to max bytes from the socket and sends them to predicate as
        // `void (const char *data, size_t len)`
        //
        // For stream sockets:
        //     The predicate may be called several times with new data, it should be considered an append function
        //     For nonblocking sockets, this will only read data currently available for reading in the socket, up to max bytes
        //        If the read would block, the predicate is not called
        //
        // For datagram sockets:
        //     The predicate is called once with the entire data of the packet, up to max bytes
        //     For nonblocking sockets, if the read would block, the predicate is not called
        //
        template<typename Predicate>
        void read(uint64_t max, Predicate predicate) {
            if (!is_connected() && !is_bound())
                throw std::logic_error("Socket can only use read() if connected or bound to an address");

            char buffer[0x1000];

            if (max > sizeof(buffer) && type() == DatagramSocket) {
                std::string packet(bytes_available(), '\0');

                while (true) {
                    const int to_read = static_cast<int>(packet.size() > max? max: packet.size());
                    const int read = ::recv(sock, &packet[0], to_read, 0);
                    if (read < 0) {
                        const int err = socket_error();
                        if (socket_would_block(err) || !handle_error(err))
                            return;
                        continue;
                    }

                    predicate(packet.c_str(), size_t(read));
                }
            } else {
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

                    predicate(static_cast<const char *>(buffer), size_t(read));
                    max -= read;
                }
            }
        }

        // Synchronously reads all data from the socket and sends it to predicate as
        // void (const char *data, size_t len);
        //
        // For stream sockets:
        //     The predicate may be called several times with new data, it should be considered an append function
        //     For blocking sockets, this will read data until the sender disconnects or shuts down its write end
        //     For nonblocking sockets, this will only read all the data currently available for reading in the socket,
        //        If the read would block, the predicate is not called
        //
        // For datagram sockets:
        //     The predicate is called once with the entire data of the packet
        //     For nonblocking sockets, if the read would block, the predicate is not called
        //
        template<typename Predicate>
        void read_all(Predicate predicate) {
            if (!is_connected() && !is_bound())
                throw std::logic_error("Socket can only use read_all() if connected or bound to an address");

            if (type() == DatagramSocket) {
                std::string packet(bytes_available(), '\0');

                while (true) {
                    const int read = ::recv(sock, &packet[0], static_cast<int>(packet.size()), 0);
                    if (read < 0) {
                        const int err = socket_error();
                        if (socket_would_block(err) || !handle_error(err))
                            return;
                        continue;
                    }

                    predicate(packet.c_str(), size_t(read));
                }
            } else {
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

                    predicate(static_cast<const char *>(buffer), size_t(read));
                }
            }
        }
        template<typename Container>
        void read_into(size_t max, Container &c) {
            read(max, [&c](const char *data, size_t len) {
                AbstractList(c) += AbstractList(data, len);
            });
        }
        void read(size_t max, InputOutputBuffer<char> &buffer) {
            read(max, [&buffer](const char *data, size_t len) {
                buffer.write(data, data + len);
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
        template<typename Container>
        Container read(size_t max) { Container result; read_into(max, result); return result; }
        template<typename Container>
        void read_all_into(Container &c) {
            read_all([&c](const char *data, size_t len) {
                AbstractList(c) += AbstractList(data, len);
            });
        }
        template<typename Container>
        Container read_all() { Container result; read_all_into(result); return result; }
        void read_all(InputOutputBuffer<char> &buffer) {
            read_all([&buffer](const char *data, size_t len) {
                buffer.write(data, data + len);
            });
        }
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

        // Returns the number of bytes waiting to be read without blocking
        size_t bytes_available() {
            if (sock == invalid_socket || is_blocking())
                return 0;

#if POSIX_OS
            int bytes = 0;
            while (::ioctl(sock, FIONREAD, &bytes) < 0 && handle_error(socket_error()))
                ;
            return size_t(bytes);
#elif WINDOWS_OS
            ULONG bytes = 0;
            while (::ioctlsocket(sock, FIONREAD, &bytes) < 0 && handle_error(socket_error()))
                ;
            return bytes;
#else
# error Platform not supported
#endif
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
#else
# error Platform not supported
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
#else
# error Platform not supported
#endif
        }

    private:
        // Performs a synchronous (blocking) bind or connect to the given address
        // If address_is_remote is true, a connect() is performed
        // If address_is_remote is false, a bind() is performed
        void bind(SocketAddress address,
                  uint16_t port,
                  bool address_is_remote) {
            static std::mutex gai_strerror_mtx;
            struct addrinfo hints;
            struct addrinfo *addresses = nullptr, *ptr = nullptr;

            status = LookingUpHost;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = address.type();
            hints.ai_socktype = type();
            hints.ai_protocol = protocol();
            hints.ai_flags = AI_PASSIVE;

            int err = 0;
            ApiString err_description;

            do {
                err = ::getaddrinfo(address? address.to_string().c_str(): NULL,
                                    port? std::to_string(port).c_str(): NULL,
                                    &hints,
                                    &addresses);

#if POSIX_OS
                if (err == EAI_SYSTEM) {
                    err = errno;
                    err_description.clear();
                } else

#endif
                /* else (if POSIX_OS) */ if (err) {
                    {
                        std::lock_guard<std::mutex> lock(gai_strerror_mtx);
#if WINDOWS_OS
                        err_description = gai_strerrorW(err);
#else
                        err_description = gai_strerror(err);
#endif
                    }

                    // Map to the most similar system errors
                    switch (err) {
#if POSIX_OS
                        case EAI_ADDRFAMILY:                  break; // No mapping
                        case EAI_NODATA:                      break; // Windows doesn't like this
#endif
                        case EAI_AGAIN:         err = EAGAIN; break;
                        case EAI_BADFLAGS:                    break; // Won't happen
                        case EAI_FAIL:                        break;
                        case EAI_FAMILY:                      break;
                        case EAI_MEMORY:        err = ENOMEM; break;
                        case EAI_NONAME:                      break;
                        case EAI_SERVICE:                     break;
                        case EAI_SOCKTYPE:                    break;
                    }
                } else {
                    err_description = {};
                }
            } while (err && handle_error(err, err_description.to_utf8().c_str()));

            status = Connecting;

            do {
                try {
                    for (ptr = addresses; ptr; ptr = ptr->ai_next) {
                        const int yes = 1;              // setsockopt option
                        bool is_broadcast = false;      // is address a broadcast address

                        switch (ptr->ai_family) {
                            default: continue;
                            case SocketAddress::IPAddressV4:
                                is_broadcast = SocketAddress(reinterpret_cast<struct sockaddr_in *>(ptr->ai_addr));
                                break;
                            case SocketAddress::IPAddressV6: break;
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

        std::deque<InputOutputBuffer<char>> wbuffer;
        InputOutputBuffer<char> rbuffer;
    };

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
        size_t read_datagram(char *data, size_t max, SocketAddress *address = nullptr, uint16_t *port = nullptr) {
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
                    if (socket_would_block(err) || !handle_error(err))
                        return 0;
                    continue;
                }

                SocketAddress sender_addr(&addr, port);
                if (address)
                    *address = sender_addr;

                return read;
            }
        }

        // Reads a datagram from the socket and sends the packet to predicate as
        // `void (const char *data, size_t len, SocketAddress address, uint16_t port)`
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
                        if (socket_would_block(err) || !handle_error(err))
                            return 0;
                        continue;
                    }

                    uint16_t port;
                    SocketAddress address(&addr, &port);
                    p(packet.c_str(), read, address, port);

                    return read;
                }
            } else {
                while (true) {
                    const int read = ::recvfrom(sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
                    if (read < 0) {
                        const int err = socket_error();
                        if (socket_would_block(err) || !handle_error(err))
                            return 0;
                        continue;
                    }

                    uint16_t port;
                    SocketAddress address(&addr, &port);
                    p(buffer, read, address, port);

                    return read;
                }
            }
        }

        std::string read_datagram(SocketAddress *address = nullptr, uint16_t *port = nullptr) {
            std::string datagram(pending_datagram_size(), '\0');
            datagram.resize(read_datagram(&datagram[0], datagram.size(), address, port));
            return datagram;
        }

        // Writes a datagram to the socket
        // The address and port of the recipient must be specified
        //
        // Returns the number of bytes written
        //
        // For nonblocking sockets, there is no way to differentiate between "failed to send" and a zero-length packet
        size_t write_datagram(SocketAddress address, uint16_t port, const char *data, size_t len) {
            if (!is_bound() && !is_connected())
                throw std::logic_error("Socket can only use write_datagram() if bound or connected to an address");

            const int yes = 1;
            struct sockaddr_storage addr;
            address.to_native(&addr, port);

            // Enable broadcast if a broadcast address
            if (address.is_broadcast() &&
                             ::setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&yes), sizeof(yes)) < 0)
                handle_error(socket_error());

            // Send packet to socket
            while (true) {
                const int to_write = len < INT_MAX? static_cast<int>(len): INT_MAX;
                const int written = ::sendto(sock, data, to_write, 0,
                                             reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
                if (written < 0) {
                    const int err = socket_error();
                    if (socket_would_block(err) || !handle_error(err))
                        return 0;
                    continue;
                }

                return written;
            }
        }
        size_t write_datagram(SocketAddress address, uint16_t port, const char *data) {
            return write_datagram(address, port, data, strlen(data));
        }
        size_t write_datagram(SocketAddress address, uint16_t port, const std::string &str) {
            return write_datagram(address, port, str.c_str(), str.length());
        }

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
}

#endif // SKATE_SOCKET_H
