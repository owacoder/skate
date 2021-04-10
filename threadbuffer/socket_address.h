#ifndef SKATE_SOCKET_ADDRESS_H
#define SKATE_SOCKET_ADDRESS_H

#include "system_includes.h"

#include <stdexcept>
#include <string>

namespace Skate {
    class SocketAddress {
    public:
        enum Type {
            IPAddressUnspecified = AF_UNSPEC, // Default, blank address
            IPAddressV4 = AF_INET,
            IPAddressV6 = AF_INET6
        };

        SocketAddress(uint16_t port = 0) : address_type(IPAddressUnspecified), address_port(port) {}
        SocketAddress(const char *address, uint16_t port = 0, Type hostname_type = IPAddressUnspecified) : address_type(IPAddressUnspecified), address_port(port) {
            if (inet_pton(IPAddressV4, address, &ipv4) > 0) {
                address_type = IPAddressV4;
            } else if (inet_pton(IPAddressV6, address, &ipv6) > 0) {
                address_type = IPAddressV6;
            } else {
                address_type = hostname_type;
                address_name = address;
            }
        }
        explicit SocketAddress(uint32_t ipv4, uint16_t port) : address_type(IPAddressV4), address_port(port), ipv4(htonl(ipv4)) {}
        explicit SocketAddress(const struct sockaddr_in *addr) : address_type(IPAddressV4), address_port(ntohs(addr->sin_port)), ipv4(addr->sin_addr.s_addr) {}
        explicit SocketAddress(const struct sockaddr_in6 *addr) : address_type(IPAddressV6), address_port(ntohs(addr->sin6_port)) {
            memcpy(ipv6, addr->sin6_addr.s6_addr, sizeof(ipv6));
        }

        static SocketAddress any(uint16_t port = 0, Type type = IPAddressV4) {
            switch (type) {
                default: return {};
                case IPAddressV4: return SocketAddress(static_cast<uint32_t>(INADDR_ANY), port);
                case IPAddressV6:
                    SocketAddress address;

                    address.address_type = type;
                    memcpy(address.ipv6, in6addr_any.s6_addr, sizeof(address.ipv6));

                    return address;
            }
        }
        static SocketAddress loopback(uint16_t port = 0, Type type = IPAddressV4) {
            switch (type) {
                default: return {};
                case IPAddressV4: return SocketAddress(static_cast<uint32_t>(INADDR_LOOPBACK), port);
                case IPAddressV6:
                    SocketAddress address;

                    address.address_type = type;
                    memcpy(address.ipv6, in6addr_loopback.s6_addr, sizeof(address.ipv6));

                    return address;
            }
        }
        static SocketAddress broadcast(uint16_t port = 0) {return SocketAddress(0xffffffffu, port);}

        Type type() const {return address_type;}
        bool is_specified() const {return address_type != IPAddressUnspecified || !address_name.empty();}
        bool is_name() const {return !address_name.empty();}
        bool is_ipv4() const {return address_type == IPAddressV4 && address_name.empty();}
        bool is_ipv6() const {return address_type == IPAddressV6 && address_name.empty();}

        bool is_any() const {
            switch (address_type) {
                default: return false;
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
        uint16_t port() const {return address_port;}

        std::string to_string(bool include_port = false) const {
            std::string result;

            switch (address_type) {
                default: {
                    if (!address_name.empty()) {
                        result = address_name;

                        if (include_port && address_port)
                            result += ":" + std::to_string(address_port);
                    }

                    break;
                }
                case IPAddressV4: {
                    char ip4[INET_ADDRSTRLEN];

                    inet_ntop(address_type, const_cast<uint32_t *>(&ipv4), ip4, sizeof(ip4));

                    result = ip4;

                    if (include_port && address_port)
                        result += ":" + std::to_string(address_port);

                    break;
                }
                case IPAddressV6: {
                    char ip6[INET6_ADDRSTRLEN];

                    inet_ntop(address_type, const_cast<void *>(static_cast<const void *>(&ipv6)), ip6, sizeof(ip6));

                    result = ip6;

                    if (include_port && address_port)
                        result = "[" + result + "]:" + std::to_string(address_port);

                    break;
                }
            }

            return result;
        }
        explicit operator std::string() const {return to_string();}

        void to_native(struct sockaddr_storage *a) {
            if (a == nullptr)
                return;

            if (!address_name.empty())
                throw std::runtime_error("Cannot convert to native IP address directly from hostname");

            switch (address_type) {
                case IPAddressV4: fill_ipv4(reinterpret_cast<struct sockaddr_in *>(a)); break;
                case IPAddressV6: fill_ipv6(reinterpret_cast<struct sockaddr_in6 *>(a)); break;
                default: throw std::runtime_error("Cannot convert to native IP address unless address is IPv4 or IPv6"); break;
            }
        }

    private:
        void fill_ipv4(struct sockaddr_in *a) {
            a->sin_family = address_type;
            a->sin_port = htons(address_port);
            a->sin_addr.s_addr = ipv4;
            memset(a->sin_zero, 0, 8);
        }
        void fill_ipv6(struct sockaddr_in6 *a) {
            a->sin6_family = address_type;
            a->sin6_port = htons(address_port);
            a->sin6_flowinfo = 0; // ???
            memcpy(a->sin6_addr.s6_addr, ipv6, sizeof(ipv6));
            a->sin6_scope_id = 0; // ???
        }

        std::string address_name;   // If non-empty, used as a hostname. Takes precedence over numeric values
        Type address_type;          // Contains address family of socket address. No address present if IPAddressUnspecified, but can have hostname and port
        uint16_t address_port;      // Host byte order
        union {
            uint32_t ipv4;          // Network byte order, big-endian
            unsigned char ipv6[16]; // Network byte order, big-endian
        };
    };
}

#endif // SKATE_SOCKET_ADDRESS_H
