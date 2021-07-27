#ifndef SKATE_SOCKET_ADDRESS_H
#define SKATE_SOCKET_ADDRESS_H

#include "../system/includes.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <system_error>

#if POSIX_OS
# include <arpa/inet.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netdb.h>
# include <fcntl.h>
#
# include <ifaddrs.h> // For getting interfaces
#elif WINDOWS_OS
# include <iphlpapi.h> // For getting interfaces
# include <ws2tcpip.h> // For inet_pton, inet_ntop
#
# if MSVC_COMPILER
#  pragma comment(lib, "iphlpapi")
# endif
#endif

namespace skate {
    enum address_type {
        IPAddressUnspecified = AF_UNSPEC, // Default, if specified and no hostname, null address
        IPAddressV4 = AF_INET,
        IPAddressV6 = AF_INET6
    };

    class socket_address {
    public:
        socket_address(uint16_t port = 0) noexcept : addr{}, port_shadow(port) {
            addr.ss_family = IPAddressUnspecified;
        }
        socket_address(uint32_t ipv4, uint16_t port = 0) noexcept : addr{}, port_shadow(port) {
            auto &v4 = ipv4_internal();
            v4.sin_family = IPAddressV4;
            v4.sin_addr.s_addr = htonl(ipv4);
            v4.sin_port = htons(port);
        }
        socket_address(const struct sockaddr_in &ipv4) noexcept : addr{}, port_shadow(ntohs(ipv4.sin_port)) {
            auto &v4 = ipv4_internal();
            v4 = ipv4;
            v4.sin_family = IPAddressV4;
        }
        socket_address(const struct sockaddr_in &ipv4, uint16_t port) noexcept : addr{}, port_shadow(port) {
            auto &v4 = ipv4_internal();
            v4 = ipv4;
            v4.sin_family = IPAddressV4;
            v4.sin_port = htons(port);
        }
        socket_address(const struct in_addr &ipv4) noexcept : addr{}, port_shadow(0) {
            auto &v4 = ipv4_internal();
            v4.sin_addr = ipv4;
            v4.sin_family = IPAddressV4;
        }
        socket_address(const struct in_addr &ipv4, uint16_t port) noexcept : addr{}, port_shadow(port) {
            auto &v4 = ipv4_internal();
            v4.sin_addr = ipv4;
            v4.sin_family = IPAddressV4;
            v4.sin_port = htons(port);
        }
        socket_address(const struct sockaddr_in6 &ipv6) noexcept : addr{}, port_shadow(ntohs(ipv6.sin6_port)) {
            auto &v6 = ipv6_internal();
            v6 = ipv6;
            v6.sin6_family = IPAddressV6;
        }
        socket_address(const struct sockaddr_in6 &ipv6, uint16_t port) noexcept : addr{}, port_shadow(port) {
            auto &v6 = ipv6_internal();
            v6 = ipv6;
            v6.sin6_family = IPAddressV6;
            v6.sin6_port = htons(port);
        }
        socket_address(const struct in6_addr &ipv6) noexcept : addr{}, port_shadow(0) {
            auto &v6 = ipv6_internal();
            v6.sin6_addr = ipv6;
            v6.sin6_family = IPAddressV6;
        }
        socket_address(const struct in6_addr &ipv6, uint16_t port) noexcept : addr{}, port_shadow(port) {
            auto &v6 = ipv6_internal();
            v6.sin6_addr = ipv6;
            v6.sin6_family = IPAddressV6;
            v6.sin6_port = htons(port);
        }
        socket_address(const struct sockaddr *addr) noexcept : addr{}, port_shadow(0) {
            if (!addr) {
                this->addr.ss_family = IPAddressUnspecified;
                return;
            }

            switch (addr->sa_family) {
                case IPAddressV4:
                    ipv4_internal() = *reinterpret_cast<const struct sockaddr_in *>(addr);
                    port_shadow = ntohs(ipv4_internal().sin_port);
                    break;
                case IPAddressV6:
                    ipv6_internal() = *reinterpret_cast<const struct sockaddr_in6 *>(addr);
                    port_shadow = ntohs(ipv6_internal().sin6_port);
                    break;
                default:
                    this->addr.ss_family = IPAddressUnspecified;
                    break;
            }
        }
        socket_address(const struct sockaddr *addr, uint16_t port) noexcept : addr{}, port_shadow(port) {
            if (!addr) {
                this->addr.ss_family = IPAddressUnspecified;
                return;
            }

            switch (addr->sa_family) {
                case IPAddressV4:
                    ipv4_internal() = *reinterpret_cast<const struct sockaddr_in *>(addr);
                    ipv4_internal().sin_port = htons(port);
                    break;
                case IPAddressV6:
                    ipv6_internal() = *reinterpret_cast<const struct sockaddr_in6 *>(addr);
                    ipv6_internal().sin6_port = htons(port);
                    break;
                default:
                    this->addr.ss_family = IPAddressUnspecified;
                    break;
            }
        }
        // Parses address and possible port
        socket_address(const char *address) : addr{}, port_shadow(0) {
            const char *open_brace = strchr(address, '[');

            if (open_brace) { // Should be an IPv6 address, [<address>]:port
                const char *close_brace = strchr(open_brace, ']');
                const char *colon = close_brace? strchr(close_brace, ':'): nullptr;

                if (colon) {
                    errno = 0;
                    char *end = nullptr;
                    const long p = strtol(colon + 1, &end, 10);
                    if (errno || p < 0 || p > 0xffff || *end != 0)
                        *this = socket_address();
                    else
                        *this = socket_address(std::string(open_brace + 1, close_brace - open_brace - 1).c_str(), uint16_t(p));
                } else if (close_brace) {
                    *this = socket_address(std::string(open_brace + 1, close_brace - open_brace - 1).c_str(), 0);
                } else {
                    *this = socket_address(address, 0);
                }
            } else { // Should be an IPv4 address, <address>:port
                const char *dot = strchr(address, '.');
                const char *colon = dot? strchr(dot, ':'): nullptr;

                if (colon) {
                    errno = 0;
                    char *end = nullptr;
                    const long p = strtol(colon + 1, &end, 10);
                    if (errno || p < 0 || p > 0xffff || *end != 0)
                        *this = socket_address();
                    else
                        *this = socket_address(std::string(address, colon - address).c_str(), uint16_t(p));
                } else {
                    *this = socket_address(address, 0);
                }
            }
        }
        // Parses strict address (no brackets) with port provided explicitly
        socket_address(const char *address, uint16_t port) noexcept : addr{}, port_shadow(port) {
            if (inet_pton(IPAddressV4, address, &ipv4_internal().sin_addr) > 0) {
                ipv4_internal().sin_family = IPAddressV4;
                ipv4_internal().sin_port = htons(port);
            } else if (inet_pton(IPAddressV6, address, &ipv6_internal().sin6_addr) > 0) {
                ipv6_internal().sin6_family = IPAddressV6;
                ipv6_internal().sin6_port = htons(port);
            } else {
                addr.ss_family = IPAddressUnspecified;
            }
        }

        address_type type() const noexcept { return static_cast<address_type>(addr.ss_family); }
        struct sockaddr_storage native() const noexcept { return addr; }
        struct sockaddr_in native_ipv4() const noexcept {
            if (is_ipv4())
                return ipv4_internal();
            else
                return socket_address(uint32_t(0), uint16_t(0)).ipv4_internal();
        }
        struct sockaddr_in6 native_ipv6() const noexcept {
            if (is_ipv6())
                return ipv6_internal();
            else
                return socket_address(in6addr_any, uint16_t(0)).ipv6_internal();
        }

        bool is_unspecified() const noexcept { return type() == IPAddressUnspecified; }
        bool is_ipv4() const noexcept { return type() == IPAddressV4; }
        bool is_ipv6() const noexcept { return type() == IPAddressV6; }

        static socket_address any(address_type type = IPAddressV4, uint16_t port = 0) noexcept {
            switch (type) {
                default: return {};
                case IPAddressV4: return socket_address(uint32_t(INADDR_ANY), port);
                case IPAddressV6: return socket_address(in6addr_any, port);
            }
        }

        static socket_address broadcast(uint16_t port = 0) noexcept {
            return socket_address(uint32_t(INADDR_BROADCAST), port);
        }

        static socket_address loopback(address_type type = IPAddressV4, uint16_t port = 0) noexcept {
            switch (type) {
                default: return {};
                case IPAddressV4: return socket_address(uint32_t(INADDR_LOOPBACK), port);
                case IPAddressV6: return socket_address(in6addr_loopback, port);
            }
        }

        bool is_any() const noexcept {
            switch (type()) {
                default: return true;
                case IPAddressV4: return ipv4_address() == INADDR_ANY;
                case IPAddressV6: return memcmp(in6addr_any.s6_addr, ipv6_internal().sin6_addr.s6_addr, sizeof(in6_addr)) == 0;
            }
        }
        bool is_broadcast() const noexcept {
            switch (type()) {
                default: return false;
                case IPAddressV4: return ipv4_address() == INADDR_BROADCAST;
            }
        }
        bool is_loopback() const noexcept {
            switch (type()) {
                default: return false;
                case IPAddressV4: return (ipv4_address() >> 24) == 127;
                case IPAddressV6: return memcmp(in6addr_loopback.s6_addr, ipv6_internal().sin6_addr.s6_addr, sizeof(in6_addr)) == 0;
            }
        }

        uint32_t ipv4_address() const noexcept {
            if (!is_ipv4())
                return 0;

            return ntohl(ipv4_internal().sin_addr.s_addr);
        }

        socket_address with_port(uint16_t port) const noexcept { return socket_address(*this).set_port(port); }
        uint16_t port() const noexcept { return port_shadow; }

        socket_address &set_port(uint16_t port) noexcept {
            port_shadow = port;

            switch (type()) {
                case IPAddressV4: ipv4_internal().sin_port = htons(port); break;
                case IPAddressV6: ipv6_internal().sin6_port = htons(port); break;
                default: break;
            }

            return *this;
        }

        std::string to_string(bool include_port = true, bool always_include_ipv6_brackets = false) const {
            switch (type()) {
                case IPAddressV4: {
                    char ip4[INET_ADDRSTRLEN];

                    inet_ntop(type(), (void *) &ipv4_internal().sin_addr, ip4, sizeof(ip4));

                    if (port_shadow && include_port)
                        return std::string(ip4) + ":" + std::to_string(port_shadow);

                    return ip4;
                }
                case IPAddressV6: {
                    char ip6[INET6_ADDRSTRLEN];

                    inet_ntop(type(), (void *) &ipv6_internal().sin6_addr, ip6, sizeof(ip6));

                    if (port_shadow && include_port)
                        return '[' + std::string(ip6) + "]:" + std::to_string(port_shadow);
                    else if (always_include_ipv6_brackets)
                        return '[' + std::string(ip6) + ']';

                    return ip6;
                }
                default:
                    return {};
            }
        }

        // Returns the local interface addresses for the local computer
        // Same as the other interfaces() function, but throws a std::system_error if an issue occurs when retrieving interfaces
        static std::vector<socket_address> interfaces(address_type type = IPAddressUnspecified, bool include_loopback = false) {
            std::error_code ec;
            auto ifaces = interfaces(ec, type, include_loopback);
            if (ec)
                throw std::system_error(ec);
            return ifaces;
        }

        // Returns the local interface addresses for the local computer
        // Only active addresses are returned, and the desired types can be filtered with the parameters to this function
        // The error code is updated with the system error that occurred while retrieving interfaces
        // Note that this function may still throw if out of memory due to vector construction
        static std::vector<socket_address> interfaces(std::error_code &ec, address_type type = IPAddressUnspecified, bool include_loopback = false) {
#if POSIX_OS
            struct ifaddrs *addresses = nullptr, *ptr = nullptr;
            std::vector<socket_address> result;

            const int err = ::getifaddrs(&addresses);
            if (err != 0) {
                ec = std::error_code(err, std::system_category());
                return {};
            }

            try {
                for (ptr = addresses; ptr; ptr = ptr->ifa_next) {
                    socket_address address;

                    switch (ptr->ifa_addr->sa_family) {
                        default: break;
                        case IPAddressV4:
                            if (type == IPAddressUnspecified || type == IPAddressV4)
                                address = socket_address{*reinterpret_cast<struct sockaddr_in *>(ptr->ifa_addr)};
                            break;
                        case IPAddressV6:
                            if (type == IPAddressUnspecified || type == IPAddressV6)
                                address = socket_address{*reinterpret_cast<struct sockaddr_in6 *>(ptr->ifa_addr)};
                            break;
                    }

                    if (!address.is_unspecified() && (!address.is_loopback() || include_loopback))
                        result.push_back(std::move(address));
                }

                freeifaddrs(addresses);
            } catch (...) {
                freeifaddrs(addresses);
                throw;
            }

            return result;
#elif WINDOWS_OS
            IP_ADAPTER_ADDRESSES *addresses = nullptr;
            ULONG addresses_buffer_size = 15000, err = 0;
            std::vector<socket_address> result;

            do {
                addresses = static_cast<IP_ADAPTER_ADDRESSES *>(malloc(addresses_buffer_size));
                if (!addresses) {
                    ec = std::make_error_code(std::errc::not_enough_memory);
                    return {};
                }

                err = ::GetAdaptersAddresses(type, 0, NULL, addresses, &addresses_buffer_size);
                if (err == ERROR_BUFFER_OVERFLOW)
                    free(addresses);
            } while (err == ERROR_BUFFER_OVERFLOW);

            if (err != ERROR_SUCCESS) {
                free(addresses);
                ec = std::error_code(err, std::system_category());
                return {};
            }

            try {
                for (auto ptr = addresses; ptr; ptr = ptr->Next) {
                    // Skip network interfaces that are not up and running
                    if (ptr->OperStatus != IfOperStatusUp)
                        continue;

                    // Skip loopback if desired
                    if (!include_loopback && ptr->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                        continue;

                    // Just look for unicast addresses
                    for (auto unicast = ptr->FirstUnicastAddress; unicast; unicast = unicast->Next) {
                        socket_address address;

                        switch (unicast->Address.lpSockaddr->sa_family) {
                            default:
                            case IPAddressV4: address = socket_address(*reinterpret_cast<struct sockaddr_in *>(unicast->Address.lpSockaddr)); break;
                            case IPAddressV6: address = socket_address(*reinterpret_cast<struct sockaddr_in6 *>(unicast->Address.lpSockaddr)); break;
                        }

                        if (!address.is_unspecified() && (!address.is_loopback() || include_loopback))
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
        struct sockaddr_in &ipv4_internal() noexcept { return *reinterpret_cast<struct sockaddr_in *>(&addr); }
        const struct sockaddr_in &ipv4_internal() const noexcept { return *reinterpret_cast<const struct sockaddr_in *>(&addr); }
        struct sockaddr_in6 &ipv6_internal() noexcept { return *reinterpret_cast<struct sockaddr_in6 *>(&addr); }
        const struct sockaddr_in6 &ipv6_internal() const noexcept { return *reinterpret_cast<const struct sockaddr_in6 *>(&addr); }

        struct sockaddr_storage addr;
        uint16_t port_shadow; // Shadows the port in `addr`, but allows for a port to be specified even on a IPAddressUnspecified address
    };

    class network_address {
    public:
        // Constructs a network address from a socket address (which is unresolved if address family is unspecified)
        network_address(socket_address address = {}) : addr(address) {}

        // Constructs a network address from a string, which may contain a port specifier as well
        network_address(const char *address) : addr(address) {
            if (addr.is_unspecified()) { // Failed to parse as IP address, must be hostname?
                const char *colon = strrchr(address, ':');

                if (colon) {
                    errno = 0;
                    char *end = nullptr;
                    const long p = strtol(colon + 1, &end, 10);
                    if (errno || p < 0 || p > 0xffff || *end != 0) {
                        name = address;
                    } else {
                        name = std::string(address, colon - address);
                        addr.set_port(uint16_t(p));
                    }
                } else {
                    name = address;
                }
            }
        }
        network_address(const std::string &address) : network_address(address.c_str()) {}

        // Returns true if the address is not specified (no network address and no hostname specified)
        bool is_unspecified() const noexcept { return addr.is_unspecified() && name.empty(); }

        // Returns true if the address has not been resolved to an endpoint yet
        bool is_unresolved() const noexcept { return addr.is_unspecified() || name.size(); }

        // Returns true if the address has been resolved to an actual endpoint
        bool is_resolved() const noexcept { return !is_unresolved(); }

        // Returns the underlying address and port, which may be unspecified if not resolved yet
        socket_address address() const noexcept { return addr; }

        network_address with_port(uint16_t port) const { return network_address(*this).set_port(port); }
        uint16_t port() const noexcept { return addr.port(); }
        network_address &set_port(uint16_t port) {
            addr.set_port(port);
            return *this;
        }

        const std::string &hostname() const noexcept { return name; }
        network_address &set_hostname(std::string hostname) {
            name = hostname;
            addr = socket_address(addr.port());
            return *this;
        }

        std::string to_string(bool include_port = true, bool always_include_ipv6_brackets = false) const {
            if (name.empty())
                return addr.to_string(include_port, always_include_ipv6_brackets);
            else if (addr.port() && include_port)
                return name + ':' + std::to_string(addr.port());
            else
                return name;
        }

    private:
        socket_address addr;
        std::string name;
    };

    class getaddrinfo_error_category : public std::error_category {
    public:
        virtual const char *name() const noexcept { return "getaddrinfo"; }
        virtual std::string message(int ev) const {
#if WINDOWS_OS
            static std::mutex gaierror_mtx;

            std::lock_guard<std::mutex> lock(gaierror_mtx);

            return to_utf8(static_cast<LPCWSTR>(::gai_strerrorW(ev)));
#else
            return ::gai_strerror(ev);
#endif
        }
    };

    class url {
        constexpr static const char *gendelims = ":/?#[]@";
        constexpr static const char *subdelims = "!$&'()*+,;=";
        constexpr static const char *pathdelims = "!$&'()*+,;=:@";
        constexpr static const char *queryfragmentdelims = "!$&'()*+,;=:@/?";

        static bool is_unreserved(unsigned char c) {
            return isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
        }

        static std::string from_percent_encoded(const std::string &s) {
            // TODO:

            return {};
        }

        static std::string to_percent_encoded(const std::string &s, const char *noescape = "") {
            std::string result;

            for (const unsigned char c: s) {
                if (is_unreserved(c) || (c && strchr(noescape, c)))
                    result.push_back(c);
                else {
                    const char hex[] = "0123456789ABCDEF";

                    result.push_back('%');
                    result.push_back(hex[c >> 4]);
                    result.push_back(hex[c & 0xf]);
                }
            }

            return result;
        }

    public:
        url() : m_valid(false) {}

        bool is_valid() const noexcept { return m_valid; }

        url &set_hostname(std::string hostname) {
            m_host = network_address(hostname);
            return *this;
        }
        url &set_host(std::string hostname) {
            m_host = network_address(hostname).with_port(m_host.port());
            return *this;
        }
        url &set_port(uint16_t port) {
            m_host.set_port(port);
            return *this;
        }
        url &set_scheme(std::string scheme) {
            m_scheme = scheme;
            return *this;
        }
        url &set_username(std::string username) {
            m_username = username;
            return *this;
        }
        url &set_password(std::string password) {
            m_password = password;
            return *this;
        }
        url &set_path(std::string path) {
            m_path = path;
            return *this;
        }
        url &set_query(std::string query) {
            m_query = query;
            return *this;
        }
        url &set_fragment(std::string fragment) {
            m_fragment = fragment;
            return *this;
        }

        std::string to_string() const {
            std::string result;

            result += m_scheme;
            result += ':';

            if (!m_host.is_unspecified() || m_host.port()) {
                result += "//";

                if (m_username.size() || m_password.size()) {
                    result += to_percent_encoded(m_username, subdelims);
                    result += ':';
                    result += to_percent_encoded(m_password, subdelims);
                    result += '@';
                }

                if (m_host.is_resolved())                   // Resolved IP address
                    result += m_host.to_string(true, true); // Automatically adds port to resolved name by default
                else {                                      // Unresolved IP address or hostname
                    result += to_percent_encoded(m_host.to_string(false), subdelims);
                    if (m_host.port()) {
                        result += ':';
                        result += std::to_string(m_host.port());
                    }
                }
            }

            result += to_percent_encoded(m_path, pathdelims);

            if (m_query.size()) {
                result += '?';
                result += to_percent_encoded(m_query, queryfragmentdelims);
            }

            if (m_fragment.size()) {
                result += '#';
                result += to_percent_encoded(m_fragment, queryfragmentdelims);
            }

            return result;
        }

    private:
        network_address m_host;
        std::string m_scheme;
        std::string m_username;
        std::string m_password;
        std::string m_path;
        std::string m_query;
        std::string m_fragment;

        bool m_valid;
    };
}

#endif // SKATE_SOCKET_ADDRESS_H
