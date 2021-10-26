/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_SOCKET_ADDRESS_H
#define SKATE_SOCKET_ADDRESS_H

#include "../system/includes.h"
#include "../containers/split_join.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <map>
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
        ip_address_unspecified = AF_UNSPEC, // Default, if specified and no hostname, null address
        ip_address_v4 = AF_INET,
        ip_address_v6 = AF_INET6
    };

    class socket_address {
    public:
        socket_address(uint16_t port = 0) noexcept : addr{}, port_shadow(port) {
            addr.ss_family = ip_address_unspecified;
        }
        socket_address(uint32_t ipv4, uint16_t port = 0) noexcept : addr{}, port_shadow(port) {
            auto &v4 = ipv4_internal();
            v4.sin_family = ip_address_v4;
            v4.sin_addr.s_addr = htonl(ipv4);
            v4.sin_port = htons(port);
        }
        socket_address(const struct sockaddr_in &ipv4) noexcept : addr{}, port_shadow(ntohs(ipv4.sin_port)) {
            auto &v4 = ipv4_internal();
            v4 = ipv4;
            v4.sin_family = ip_address_v4;
        }
        socket_address(const struct sockaddr_in &ipv4, uint16_t port) noexcept : addr{}, port_shadow(port) {
            auto &v4 = ipv4_internal();
            v4 = ipv4;
            v4.sin_family = ip_address_v4;
            v4.sin_port = htons(port);
        }
        socket_address(const struct in_addr &ipv4) noexcept : addr{}, port_shadow(0) {
            auto &v4 = ipv4_internal();
            v4.sin_addr = ipv4;
            v4.sin_family = ip_address_v4;
        }
        socket_address(const struct in_addr &ipv4, uint16_t port) noexcept : addr{}, port_shadow(port) {
            auto &v4 = ipv4_internal();
            v4.sin_addr = ipv4;
            v4.sin_family = ip_address_v4;
            v4.sin_port = htons(port);
        }
        socket_address(const struct sockaddr_in6 &ipv6) noexcept : addr{}, port_shadow(ntohs(ipv6.sin6_port)) {
            auto &v6 = ipv6_internal();
            v6 = ipv6;
            v6.sin6_family = ip_address_v6;
        }
        socket_address(const struct sockaddr_in6 &ipv6, uint16_t port) noexcept : addr{}, port_shadow(port) {
            auto &v6 = ipv6_internal();
            v6 = ipv6;
            v6.sin6_family = ip_address_v6;
            v6.sin6_port = htons(port);
        }
        socket_address(const struct in6_addr &ipv6) noexcept : addr{}, port_shadow(0) {
            auto &v6 = ipv6_internal();
            v6.sin6_addr = ipv6;
            v6.sin6_family = ip_address_v6;
        }
        socket_address(const struct in6_addr &ipv6, uint16_t port) noexcept : addr{}, port_shadow(port) {
            auto &v6 = ipv6_internal();
            v6.sin6_addr = ipv6;
            v6.sin6_family = ip_address_v6;
            v6.sin6_port = htons(port);
        }
        socket_address(const struct sockaddr *addr) noexcept : addr{}, port_shadow(0) {
            if (!addr) {
                this->addr.ss_family = ip_address_unspecified;
                return;
            }

            switch (addr->sa_family) {
                case ip_address_v4:
                    ipv4_internal() = *reinterpret_cast<const struct sockaddr_in *>(addr);
                    port_shadow = ntohs(ipv4_internal().sin_port);
                    break;
                case ip_address_v6:
                    ipv6_internal() = *reinterpret_cast<const struct sockaddr_in6 *>(addr);
                    port_shadow = ntohs(ipv6_internal().sin6_port);
                    break;
                default:
                    this->addr.ss_family = ip_address_unspecified;
                    break;
            }
        }
        socket_address(const struct sockaddr *addr, uint16_t port) noexcept : addr{}, port_shadow(port) {
            if (!addr) {
                this->addr.ss_family = ip_address_unspecified;
                return;
            }

            switch (addr->sa_family) {
                case ip_address_v4:
                    ipv4_internal() = *reinterpret_cast<const struct sockaddr_in *>(addr);
                    ipv4_internal().sin_port = htons(port);
                    break;
                case ip_address_v6:
                    ipv6_internal() = *reinterpret_cast<const struct sockaddr_in6 *>(addr);
                    ipv6_internal().sin6_port = htons(port);
                    break;
                default:
                    this->addr.ss_family = ip_address_unspecified;
                    break;
            }
        }
        socket_address(const struct sockaddr_storage &addr) noexcept : socket_address(reinterpret_cast<const struct sockaddr *>(&addr)) {}
        socket_address(const struct sockaddr_storage &addr, uint16_t port) noexcept : socket_address(reinterpret_cast<const struct sockaddr *>(&addr), port) {}
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
                    if (errno || p > 0xffff || p < 0 || end <= colon + 1 || *end != 0) {
                        *this = socket_address();
                    } else {
                        *this = socket_address(std::string(open_brace + 1, close_brace - open_brace - 1).c_str(), uint16_t(p));
                    }
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
                    if (errno || p > 0xffff || p < 0 || end <= colon + 1 || *end != 0) {
                        *this = socket_address();
                    } else {
                        *this = socket_address(std::string(address, colon - address).c_str(), uint16_t(p));
                    }
                } else {
                    *this = socket_address(address, 0);
                }
            }
        }
        // Parses strict address (no brackets) with port provided explicitly
        socket_address(const char *address, uint16_t port) noexcept : addr{}, port_shadow(port) {
            if (inet_pton(ip_address_v4, address, &ipv4_internal().sin_addr) > 0) {
                ipv4_internal().sin_family = ip_address_v4;
                ipv4_internal().sin_port = htons(port);
            } else if (inet_pton(ip_address_v6, address, &ipv6_internal().sin6_addr) > 0) {
                ipv6_internal().sin6_family = ip_address_v6;
                ipv6_internal().sin6_port = htons(port);
            } else {
                addr.ss_family = ip_address_unspecified;
            }
        }

        address_type type() const noexcept { return static_cast<address_type>(addr.ss_family); }
        const struct sockaddr_storage &native_storage() const noexcept { return addr; }
        const struct sockaddr *native() const noexcept { return reinterpret_cast<const struct sockaddr *>(&addr); }
        socklen_t native_length() const noexcept { return sizeof(addr); }
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

        bool is_unspecified() const noexcept { return type() == ip_address_unspecified; }
        bool is_ipv4() const noexcept { return type() == ip_address_v4; }
        bool is_ipv6() const noexcept { return type() == ip_address_v6; }

        bool is_fully_resolved() const noexcept {
            switch (type()) {
                case ip_address_v4:
                case ip_address_v6: return port() != 0;
                default: return false;
            }
        }

        static socket_address any(address_type type = ip_address_v4, uint16_t port = 0) noexcept {
            switch (type) {
                default: return {};
                case ip_address_v4: return socket_address(uint32_t(INADDR_ANY), port);
                case ip_address_v6: return socket_address(in6addr_any, port);
            }
        }

        static socket_address broadcast(uint16_t port = 0) noexcept {
            return socket_address(uint32_t(INADDR_BROADCAST), port);
        }

        static socket_address loopback(address_type type = ip_address_v4, uint16_t port = 0) noexcept {
            switch (type) {
                default: return {};
                case ip_address_v4: return socket_address(uint32_t(INADDR_LOOPBACK), port);
                case ip_address_v6: return socket_address(in6addr_loopback, port);
            }
        }

        bool is_any() const noexcept {
            switch (type()) {
                default: return true;
                case ip_address_v4: return ipv4_address() == INADDR_ANY;
                case ip_address_v6: return memcmp(in6addr_any.s6_addr, ipv6_internal().sin6_addr.s6_addr, sizeof(in6_addr)) == 0;
            }
        }
        bool is_broadcast() const noexcept {
            switch (type()) {
                default: return false;
                case ip_address_v4: return ipv4_address() == INADDR_BROADCAST;
            }
        }
        bool is_loopback() const noexcept {
            switch (type()) {
                default: return false;
                case ip_address_v4: return (ipv4_address() >> 24) == 127;
                case ip_address_v6: return memcmp(in6addr_loopback.s6_addr, ipv6_internal().sin6_addr.s6_addr, sizeof(in6_addr)) == 0;
            }
        }

        uint32_t ipv4_address() const noexcept {
            if (!is_ipv4())
                return 0;

            return ntohl(ipv4_internal().sin_addr.s_addr);
        }

        socket_address with_port(uint16_t port) const noexcept { return socket_address(*this).set_port(port); }
        uint16_t port(uint16_t default_port = 0) const noexcept { return port_shadow? port_shadow: default_port; }

        socket_address &set_port(uint16_t port) noexcept {
            port_shadow = port;

            switch (type()) {
                case ip_address_v4: ipv4_internal().sin_port = htons(port); break;
                case ip_address_v6: ipv6_internal().sin6_port = htons(port); break;
                default: break;
            }

            return *this;
        }

        std::string to_string(bool include_port = true, bool always_include_ipv6_brackets = false) const {
            switch (type()) {
                case ip_address_v4: {
                    char ip4[INET_ADDRSTRLEN];

                    inet_ntop(type(), (void *) &ipv4_internal().sin_addr, ip4, sizeof(ip4));

                    if (port_shadow && include_port)
                        return std::string(ip4) + ":" + std::to_string(port_shadow);

                    return ip4;
                }
                case ip_address_v6: {
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
        static std::vector<socket_address> interfaces(address_type type = ip_address_unspecified, bool include_loopback = false) {
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
        static std::vector<socket_address> interfaces(std::error_code &ec, address_type type = ip_address_unspecified, bool include_loopback = false) {
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
                        case ip_address_v4:
                            if (type == ip_address_unspecified || type == ip_address_v4)
                                address = socket_address{*reinterpret_cast<struct sockaddr_in *>(ptr->ifa_addr)};
                            break;
                        case ip_address_v6:
                            if (type == ip_address_unspecified || type == ip_address_v6)
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
                ec = std::error_code(err, win32_category());
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
                            case ip_address_v4: address = socket_address(*reinterpret_cast<struct sockaddr_in *>(unicast->Address.lpSockaddr)); break;
                            case ip_address_v6: address = socket_address(*reinterpret_cast<struct sockaddr_in6 *>(unicast->Address.lpSockaddr)); break;
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
                    if (errno || p > 0xffff || p < 0 || end <= colon + 1 || *end != 0) {
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
        network_address(const char *address, uint16_t port) : addr(address, port) {
            if (addr.is_unspecified()) // Failed to parse as IP address, must be hostname?
                name = address;
        }
        network_address(const std::string &address) : network_address(address.c_str()) {}
        network_address(const std::string &address, uint16_t port) : network_address(address.c_str(), port) {}

        // Returns true if the address is null (no network address and no hostname specified)
        bool is_null() const noexcept { return addr.is_unspecified() && name.empty(); }
        bool is_hostname() const noexcept { return !name.empty(); }
        bool is_resolved() const noexcept { return !addr.is_unspecified() && name.empty(); }

        // Returns the underlying address and port, which may be unspecified if not resolved yet
        socket_address address() const noexcept { return addr; }

        network_address with_port(uint16_t port) const { return network_address(*this).set_port(port); }
        uint16_t port(uint16_t default_port = 0) const noexcept { return addr.port(default_port); }
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

    // TODO: not fully implemented yet
    // https://datatracker.ietf.org/doc/html/rfc3986
    class url {
    public:
        enum class encoding {
            raw,
            percent
        };

    private:
        constexpr static const char *gendelims = ":/?#[]@";
        constexpr static const char *subdelims = "!$&'()*+,;=";
        constexpr static const char *pathdelims = "!$&'()*+,;=:@";
        constexpr static const char *pathdelimswithslash = "!$&'()*+,;=:@/";
        constexpr static const char *queryfragmentdelims = "!$&'()*+,;=:@/?";
        constexpr static const char *querymapdelims = "!$'()*+,;:@/?"; // No '=' or '&' to force escaping those characters in key/value queries

        static bool is_unreserved(unsigned char c) {
            return isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
        }

#if __cplusplus >= 201703L
        static std::string from_string_helper(std::string_view s, encoding fmt, bool *error = nullptr) {
#else
        static std::string from_string_helper(const std::string &s, encoding fmt, bool *error = nullptr) {
#endif
            std::string result;

            if (error)
                *error = false;

            if (fmt == encoding::raw)
                return std::string(s);

            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '%') {
                    if (i + 2 < s.size() && isxdigit(s[i+1] & 0xff) && isxdigit(s[i+2] & 0xff)) {
                        result.push_back((toxdigit(s[i+1]) << 4) | toxdigit(s[i+2]));
                        i += 2;
                        continue;
                    } else {
                        if (error)
                            *error = true;
                    }
                }

                result.push_back(s[i]);
            }

            return result;
        }

        static void to_percent_encoded(std::string &append_to, const std::string &s, const char *noescape = "") {
            for (const unsigned char c: s) {
                if (is_unreserved(c) || (c && strchr(noescape, c)))
                    append_to.push_back(c);
                else {
                    append_to.push_back('%');
                    append_to.push_back(toxchar(c >> 4, true));
                    append_to.push_back(toxchar(c     , true));
                }
            }
        }

        void append_scheme(std::string &append_to) const {
            append_to += m_scheme;
        }

        void append_username(std::string &append_to, encoding fmt) const {
            switch (fmt) {
                default:                append_to += m_username; break;
                case encoding::percent: to_percent_encoded(append_to, m_username, subdelims); break;
            }
        }

        void append_password(std::string &append_to, encoding fmt) const {
            switch (fmt) {
                default:                append_to += m_password; break;
                case encoding::percent: to_percent_encoded(append_to, m_password, subdelims); break;
            }
        }

        // Userinfo is user:password
        void append_userinfo(std::string &append_to, encoding fmt) const {
            if (m_username.size() || m_password.size()) {
                append_username(append_to, fmt);
                append_to += ':';
                append_password(append_to, fmt);
            }
        }

        void append_host(std::string &append_to, encoding fmt) const {
            if (m_host.is_resolved())                       // Resolved IP address
                append_to += m_host.to_string(false, true); // Automatically adds port to resolved name by default
            else {                                          // Unresolved IP address or hostname
                switch (fmt) {
                    default:                append_to += m_host.to_string(false); break;
                    case encoding::percent: to_percent_encoded(append_to, m_host.to_string(false), subdelims); break;
                }
            }
        }

        void append_port(std::string &append_to) const {
            if (m_host.port()) {
                append_to += ':';
                append_to += std::to_string(m_host.port());
            }
        }

        // Hostname is host [:port]
        void append_hostname(std::string &append_to, encoding fmt) const {
            append_host(append_to, fmt);
            append_port(append_to);
        }

        // Authority is [userinfo@] hostname
        void append_authority(std::string &append_to, encoding fmt) const {
            if (m_username.size() || m_password.size()) {
                append_username(append_to, fmt);
                append_to += ':';
                append_password(append_to, fmt);
                append_to += '@';
            }

            append_hostname(append_to, fmt);
        }

        void append_path(std::string &append_to, encoding fmt) const {
            if (m_pathlist.size()) {
                join_append(append_to, m_pathlist, "/", [fmt](std::string &append_to, const std::string &path_element) {
                    switch (fmt) {
                        default:                append_to += path_element; break;
                        case encoding::percent: to_percent_encoded(append_to, path_element, pathdelims); break;
                    }
                });
            } else {
                switch (fmt) {
                    default:                append_to += m_path; break;
                    case encoding::percent: to_percent_encoded(append_to, m_path, pathdelimswithslash); break;
                }
            }
        }

        void append_query(std::string &append_to, encoding fmt) const {
            if (m_querymap.size()) {
                join_append(append_to, m_querymap, "&", [fmt](std::string &append_to_element, const std::pair<std::string, std::string> &query_element) {
                    switch (fmt) {
                        default:                append_to_element += query_element.first; break;
                        case encoding::percent: to_percent_encoded(append_to_element, query_element.first, querymapdelims); break;
                    }
                    append_to_element += '=';
                    switch (fmt) {
                        default:                append_to_element += query_element.second; break;
                        case encoding::percent: to_percent_encoded(append_to_element, query_element.second, querymapdelims); break;
                    }
                });
            } else if (m_query.size()) {
                switch (fmt) {
                    default:                append_to += m_query; break;
                    case encoding::percent: to_percent_encoded(append_to, m_query, queryfragmentdelims); break;
                }
            }
        }

        void append_fragment(std::string &append_to, encoding fmt) const {
            switch (fmt) {
                default:                append_to += m_fragment; break;
                case encoding::percent: to_percent_encoded(append_to, m_fragment, queryfragmentdelims); break;
            }
        }

        // Consists of path?query, or whatever exists
        void append_path_and_query(std::string &append_to, encoding fmt) const {
            append_path(append_to, fmt);

            if (has_query()) {
                append_to += '?';
                append_query(append_to, fmt);
            }
        }

        // Consists of path?query#fragment, or whatever exists
        void append_path_and_query_and_fragment(std::string &append_to, encoding fmt) const {
            append_path_and_query(append_to, fmt);

            if (has_fragment()) {
                append_to += '#';
                append_fragment(append_to, fmt);
            }
        }

    public:
        url() {}
#if __cplusplus >= 201703L
        url(std::string_view s, encoding fmt = encoding::percent) { *this = from_string(s, fmt); }
#else
        url(const std::string &s, encoding fmt = encoding::percent) { *this = from_string(s, fmt); }
#endif

        bool valid() const noexcept {
            if (m_scheme.empty() || !isalpha(m_scheme[0] & 0xff))
                return false;

            for (const unsigned char c: m_scheme)
                if (!isalnum(c) && c != '+' && c != '-' && c != '.')
                    return false;

            if (m_host.is_null())
                return false;

            if (has_authority() && get_path().substr(0, 2) == "//")
                return false;

            return true;
        }
        operator bool() const noexcept { return valid(); }

        bool has_host() const noexcept { return !m_host.is_null(); }
        bool has_port() const noexcept { return m_host.port(); }
        bool has_username() const noexcept { return m_username.size(); }
        bool has_password() const noexcept { return m_password.size(); }
        bool has_scheme() const noexcept { return m_scheme.size(); }
        bool has_query() const noexcept { return m_query.size(); }
        bool has_fragment() const noexcept { return m_fragment.size(); }

        bool has_hostname() const noexcept { return has_host() || has_port(); }
        bool has_userinfo() const noexcept { return has_username() || has_password(); }
        bool has_authority() const noexcept { return has_userinfo() || has_hostname(); }

        std::string get_host(encoding fmt = encoding::raw) const { std::string result; append_host(result, fmt); return result; }
        uint16_t get_port(uint16_t default_port = 0) const noexcept { return m_host.port(default_port); }
        std::string get_hostname(encoding fmt = encoding::raw) const { std::string result; append_hostname(result, fmt); return result; }
        std::string get_username(encoding fmt = encoding::raw) const { std::string result; append_username(result, fmt); return result; }
        std::string get_password(encoding fmt = encoding::raw) const { std::string result; append_password(result, fmt); return result; }
        std::string get_userinfo(encoding fmt = encoding::raw) const { std::string result; append_userinfo(result, fmt); return result; }
        std::string get_authority(encoding fmt = encoding::raw) const { std::string result; append_authority(result, fmt); return result; }
        std::string get_scheme() const { std::string result; append_scheme(result); return result; }
        std::string get_path(encoding fmt = encoding::raw) const { std::string result; append_path(result, fmt); return result; }
        std::string get_query(encoding fmt = encoding::raw) const { std::string result; append_query(result, fmt); return result; }
        std::string get_fragment(encoding fmt = encoding::raw) const { std::string result; append_fragment(result, fmt); return result; }
        std::string get_path_and_query(encoding fmt = encoding::raw) const { std::string result; append_path_and_query(result, fmt); return result; }
        std::string get_path_and_query_and_fragment(encoding fmt = encoding::raw) const { std::string result; append_path_and_query_and_fragment(result, fmt); return result; }

        size_t path_elements() const {
            if (m_path.size()) {
                m_pathlist = split(m_path, "/");
                m_path.clear();
            }

            return m_pathlist.size();
        }

        url &set_hostname(std::string hostname) {
            m_host = network_address(std::move(hostname));
            return *this;
        }
        url &set_host(std::string hostname) {
            m_host = network_address(std::move(hostname)).with_port(m_host.port());
            return *this;
        }
        url &set_port(uint16_t port) {
            m_host.set_port(port);
            return *this;
        }
        url &set_authority(std::string authority, encoding fmt = encoding::raw) {
            size_t start = 0;
            const size_t end = authority.find('@');

            if (end != authority.npos) {
                const size_t middle = authority.find(':');

                if (middle == authority.npos) { // Username only
                    set_username(authority.substr(start, end - start), fmt);
                } else { // Username and password
                    set_username(authority.substr(start, middle - start), fmt);
                    start = middle + 1;
                    set_password(authority.substr(start, end - start), fmt);
                }

                start = end + 1;
            }

            return set_hostname(authority.substr(start));
        }
        url &set_scheme(std::string scheme, encoding fmt = encoding::raw) {
            m_scheme = from_string_helper(scheme, fmt);
            return *this;
        }
        url &set_username(std::string username, encoding fmt = encoding::raw) {
            m_username = from_string_helper(username, fmt);
            return *this;
        }
        url &set_password(std::string password, encoding fmt = encoding::raw) {
            m_password = from_string_helper(password, fmt);
            return *this;
        }
        url &set_path(std::string path, encoding fmt = encoding::raw) {
            m_path = from_string_helper(path, fmt);
            m_pathlist = {};
            return *this;
        }
        url &set_path(std::vector<std::string> path, encoding fmt = encoding::raw) {
            for (auto &p: path)
                p = from_string_helper(p, fmt);

            m_path.clear();
            m_pathlist = std::move(path);
            return *this;
        }
        url &set_query(std::string query, encoding fmt = encoding::raw) {
            m_query = from_string_helper(query, fmt);
            m_querymap = {};
            return *this;
        }
        url &set_queries(std::map<std::string, std::string> querymap) {
            m_query.clear();
            m_querymap = std::move(querymap);
            return *this;
        }
        url &clear_queries() { return set_query(std::string{}); }
        url &set_query(std::string key, std::string value, encoding fmt = encoding::raw) {
            if (m_query.size()) {
                std::vector<std::string> queries = split(m_query, "&");

                for (const auto &query: queries) {
                    const auto equals = query.find('=');

                    if (equals == query.npos) { // Not a valid key/value pair, assume that the rest are not valid too
                        m_querymap.clear();
                        break;
                    }

                    m_querymap[from_string_helper(query.substr(0, equals), fmt)] = from_string_helper(query.substr(equals + 1), fmt);
                }
            }

            m_querymap[from_string_helper(key, fmt)] = from_string_helper(value, fmt);
            return *this;
        }
        url &set_fragment(std::string fragment, encoding fmt = encoding::raw) {
            m_fragment = from_string_helper(fragment, fmt);
            return *this;
        }

        // Creates a string with the stored URL, but doesn't verify its validity
        std::string to_string(encoding fmt = encoding::percent) const {
            std::string result;

            if (m_scheme.size()) {
                result += m_scheme;
                result += ':';
            }

            if (has_authority()) {
                result += "//";
                append_authority(result, fmt);
            }

            append_path_and_query_and_fragment(result, fmt);

            return result;
        }

        // Creates a URL from the given string, but doesn't verify its validity
#if __cplusplus >= 201703L
        static url from_string(std::string_view s, encoding fmt = encoding::percent) {
#else
        static url from_string(const std::string &s, encoding fmt = encoding::percent) {
#endif
            url result;

            if (s.empty())
                return result;

            size_t start = 0;
            size_t end = s.find(':', start);
            if (end == s.npos)
                return result;

            // Parse scheme (all lowercase)
            result.set_scheme(std::string(s.substr(start, end - start)), fmt);
            start = end + 1;

            for (size_t i = 0; i < result.m_scheme.size(); ++i)
                result.m_scheme[i] = tolower(result.m_scheme[i] & 0xff);

            // Parse authority
            if (s.size() - start > 2 && s[start] == '/' && s[start+1] == '/') {
                start += 2;
                end = s.find_first_of("/?#", start);

                result.set_authority(std::string(s.substr(start, end - start)), fmt);
                if (end == s.npos)
                    return result;

                start = end;
            }

            // Parse path if present
            if (start < s.size() && s[start] == '/') {
                end = s.find_first_of("?#", ++start);

                result.set_path(std::string(s.substr(start, end - start)), fmt);
                start = end;
            }

            // Parse query if present
            if (start < s.size() && s[start] == '?') {
                end = s.find('#', ++start);

                result.set_query(std::string(s.substr(start, end - start)), fmt);
                start = end;
            }

            // Parse fragment if present
            if (start < s.size() && s[start] == '#') {
                ++start;

                result.set_fragment(std::string(s.substr(start, end - start)), fmt);
            }

            return result;
        }

    private:
        network_address m_host;
        std::string m_scheme;
        std::string m_username;
        std::string m_password;

        // Invariant is maintained that (m_path.size() && m_pathlist.size()) == false
        mutable std::string m_path;
        mutable std::vector<std::string> m_pathlist;

        // Invariant is maintained that (m_query.size() && m_querymap.size()) == false
        mutable std::string m_query;
        mutable std::map<std::string, std::string> m_querymap;

        std::string m_fragment;
    };
}

#endif // SKATE_SOCKET_ADDRESS_H
