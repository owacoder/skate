#ifndef SKATE_SOCKET_H
#define SKATE_SOCKET_H

#include "socket_address.h"

#include <vector>

// See https://beej.us/guide/bgnet/html

namespace Skate {
    class Socket {
    protected:
        SocketDescriptor socket;

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

        struct AddressInfo {
            AddressInfo()
                : type(AnySocket)
                , protocol(AnyProtocol)
            {}
            explicit AddressInfo(SocketAddress address,
                        Type type = AnySocket,
                        Protocol protocol = AnyProtocol)
                : address(address)
                , type(type)
                , protocol(protocol)
            {}

            operator SocketAddress() const {
                return address;
            }

            SocketAddress address;
            Type type;
            Protocol protocol;
        };

        Socket() {}
        virtual ~Socket() {}

        // Returns addresses that can be bound to a local socket for serving clients
        static std::vector<AddressInfo> bindable_server_addresses(Type socket_type,
                                                                  Protocol protocol_type,
                                                                  SocketAddress address = {}) {
            return address_info(socket_type, protocol_type, address, AI_PASSIVE);
        }
        std::vector<AddressInfo> bindable_server_addresses(SocketAddress address = {}) {
            return bindable_server_addresses(type(), protocol(), address);
        }

        // Resolves a remote address or hostname for use as a client
        static std::vector<AddressInfo> remote_server_addresses(Type socket_type,
                                                                Protocol protocol_type,
                                                                SocketAddress address = {})
        {
            return address_info(socket_type, protocol_type, address);
        }
        std::vector<AddressInfo> remote_server_addresses(SocketAddress address = {}) {
            return remote_server_addresses(type(), protocol(), address);
        }

        // Reimplement in client to override behavior of address resolution
        virtual Type type() const {return AnySocket;}
        virtual Protocol protocol() const {return AnyProtocol;}

    private:
        static std::vector<AddressInfo> address_info(Type socket_type,
                                                     Protocol protocol_type,
                                                     SocketAddress address,
                                                     int flags = 0,
                                                     int *error = nullptr) {
            std::vector<AddressInfo> result;

            struct addrinfo hints;
            struct addrinfo *addresses, *ptr;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = address.type();
            hints.ai_socktype = socket_type;
            hints.ai_protocol = protocol_type;
            hints.ai_flags = flags;

            const int ai_info_result = getaddrinfo(address.is_specified()? address.to_string().c_str(): NULL,
                                                   std::to_string(address.port()).c_str(),
                                                   &hints,
                                                   &addresses);
            if (ai_info_result != 0) {
                if (error)
                    *error = ai_info_result;
                else
                    throw std::runtime_error(system_error_string(ai_info_result).to_utf8());
            }

            for (ptr = addresses; ptr; ptr = ptr->ai_next) {
                SocketAddress address;

                switch (ptr->ai_family) {
                    default: continue; // Don't add to result list if unknown family
                    case SocketAddress::IPAddressV4: address = SocketAddress(reinterpret_cast<const struct sockaddr_in *>(ptr->ai_addr)); break;
                    case SocketAddress::IPAddressV6: address = SocketAddress(reinterpret_cast<const struct sockaddr_in6 *>(ptr->ai_addr)); break;
                }

                result.push_back(AddressInfo(address,
                                             static_cast<Type>(ptr->ai_socktype),
                                             static_cast<Protocol>(ptr->ai_protocol)));
            }

            freeaddrinfo(addresses);

            return result;
        }
    };

    class UDPSocket : public Socket {
    public:
        UDPSocket() {}

        virtual Type type() const {return DatagramSocket;}
        virtual Protocol protocol() const {return UDPProtocol;}
    };

    class TCPSocket : public Socket {
    public:
        TCPSocket() {}

        virtual Type type() const {return StreamSocket;}
        virtual Protocol protocol() const {return TCPProtocol;}
    };
}

#endif // SKATE_SOCKET_H
