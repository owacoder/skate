#ifndef SKATE_SOCKET_ADDRESS_H
#define SKATE_SOCKET_ADDRESS_H

#include "../system/includes.h"

#include <stdexcept>
#include <string>

namespace Skate {
    class SocketAddress {
    public:
        enum Type {
            IPAddressUnspecified = AF_UNSPEC, // Default, if specified and no hostname, null address
            IPAddressV4 = AF_INET,
            IPAddressV6 = AF_INET6
        };

        SocketAddress() : address_type(IPAddressUnspecified) {}
        SocketAddress(const char *address, Type hostname_type = IPAddressUnspecified) : address_type(IPAddressUnspecified) {
            if (inet_pton(IPAddressV4, address, &ipv4) > 0) {
                address_type = IPAddressV4;
            } else if (inet_pton(IPAddressV6, address, &ipv6) > 0) {
                address_type = IPAddressV6;
            } else {
                address_type = hostname_type;
                address_name = address;
            }
        }
        explicit SocketAddress(uint32_t ipv4) : address_type(IPAddressV4), ipv4(htonl(ipv4)) {}
        explicit SocketAddress(const struct sockaddr_storage *addr) : address_type(static_cast<Type>(addr->ss_family)) {
            switch (addr->ss_family) {
                default: address_type = IPAddressUnspecified; break;
                case IPAddressV4: ipv4 = reinterpret_cast<const struct sockaddr_in *>(addr)->sin_addr.s_addr; break;
                case IPAddressV6: memcpy(ipv6, reinterpret_cast<const struct sockaddr_in6 *>(addr)->sin6_addr.s6_addr, sizeof(ipv6)); break;
            }
        }
        explicit SocketAddress(const struct sockaddr_in *addr) : address_type(IPAddressV4), ipv4(addr->sin_addr.s_addr) {}
        explicit SocketAddress(const struct sockaddr_in6 *addr) : address_type(IPAddressV6) {
            memcpy(ipv6, addr->sin6_addr.s6_addr, sizeof(ipv6));
        }

        static SocketAddress any(Type type = IPAddressV4) {
            switch (type) {
                default: return {};
                case IPAddressV4: return SocketAddress(static_cast<uint32_t>(INADDR_ANY));
                case IPAddressV6:
                    SocketAddress address;

                    address.address_type = type;
                    memcpy(address.ipv6, in6addr_any.s6_addr, sizeof(address.ipv6));

                    return address;
            }
        }
        static SocketAddress loopback(Type type = IPAddressV4) {
            switch (type) {
                default: return {};
                case IPAddressV4: return SocketAddress(static_cast<uint32_t>(INADDR_LOOPBACK));
                case IPAddressV6:
                    SocketAddress address;

                    address.address_type = type;
                    memcpy(address.ipv6, in6addr_loopback.s6_addr, sizeof(address.ipv6));

                    return address;
            }
        }
        static SocketAddress broadcast() {return SocketAddress(0xffffffffu);}

        Type type() const {return address_type;}
        bool is_null() const {return address_name.empty() && is_any_family();}
        bool is_hostname() const {return !address_name.empty();}
        bool is_any_family() const {return address_type == IPAddressUnspecified;}
        bool is_ipv4() const {return address_type == IPAddressV4 && address_name.empty();}
        bool is_ipv6() const {return address_type == IPAddressV6 && address_name.empty();}

        bool is_any() const {
            switch (address_type) {
                default: return true;
                case IPAddressV4: return ipv4 == htonl(INADDR_ANY);
                case IPAddressV6: return memcmp(in6addr_any.s6_addr, ipv6, sizeof(ipv6)) == 0;
            }
        }
        bool is_broadcast() const {
            switch (address_type) {
                default: return false;
                case IPAddressV4: return ipv4 == htonl(INADDR_BROADCAST);
            }
        }
        bool is_loopback() const {
            switch (address_type) {
                default: return false;
                case IPAddressV4: return ipv4 == htonl(INADDR_LOOPBACK);
                case IPAddressV6: return memcmp(in6addr_loopback.s6_addr, ipv6, sizeof(ipv6)) == 0;
            }
        }

        uint32_t ipv4_address() const {
            switch (address_type) {
                default: return 0;
                case IPAddressV4: return ntohl(ipv4);
            }
        }

        std::string to_string(uint16_t port = 0) const {
            std::string result = address_name;

            if (!result.empty()) {
                if (port)
                    result += ":" + std::to_string(port);

                return result;
            }

            switch (address_type) {
                default: return port? ":" + std::to_string(port): "";
                case IPAddressV4: {
                    char ip4[INET_ADDRSTRLEN];

                    inet_ntop(address_type, const_cast<uint32_t *>(&ipv4), ip4, sizeof(ip4));

                    if (port)
                        return std::string(ip4) + ":" + std::to_string(port);

                    return ip4;
                }
                case IPAddressV6: {
                    char ip6[INET6_ADDRSTRLEN];

                    inet_ntop(address_type, const_cast<void *>(static_cast<const void *>(&ipv6)), ip6, sizeof(ip6));

                    if (port)
                        return "[" + std::string(ip6) + "]:" + std::to_string(port);

                    return ip6;
                }
            }
        }
        explicit operator std::string() const {return to_string();}
        operator bool() const {return !is_null();}

        void to_native(struct sockaddr_storage *a, uint16_t port = 0) {
            if (a == nullptr)
                return;

            if (!address_name.empty())
                throw std::logic_error("Cannot convert to native IP address directly from hostname");

            switch (address_type) {
                case IPAddressV4: fill_ipv4(reinterpret_cast<struct sockaddr_in *>(a), port); break;
                case IPAddressV6: fill_ipv6(reinterpret_cast<struct sockaddr_in6 *>(a), port); break;
                default: throw std::logic_error("Cannot convert to native IP address unless address is IPv4 or IPv6"); break;
            }
        }

    private:
        void fill_ipv4(struct sockaddr_in *a, uint16_t port) {
            a->sin_family = address_type;
            a->sin_port = htons(port);
            a->sin_addr.s_addr = ipv4;
            memset(a->sin_zero, 0, 8);
        }
        void fill_ipv6(struct sockaddr_in6 *a, uint16_t port) {
            a->sin6_family = address_type;
            a->sin6_port = htons(port);
            a->sin6_flowinfo = 0; // ???
            memcpy(a->sin6_addr.s6_addr, ipv6, sizeof(ipv6));
            a->sin6_scope_id = 0; // ???
        }

        std::string address_name;   // If non-empty, used as a hostname. Takes precedence over numeric values
        Type address_type;          // Contains address family of socket address. No address present if IPAddressUnspecified, but can have hostname and port
        union {
            uint32_t ipv4;          // Network byte order, big-endian
            unsigned char ipv6[16]; // Network byte order, big-endian
        };
    };
}

#endif // SKATE_SOCKET_ADDRESS_H
