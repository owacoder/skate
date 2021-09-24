/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_SERVER_H
#define SKATE_SERVER_H

#include "socket.h"

#include "select.h"
#include "poll.h"
#include "epoll.h"
#include "wsaasyncselect.h"

#include <memory>
#include <unordered_map>
#include <utility>
#include <thread>

#include <type_traits>

namespace skate {
    namespace impl {
#if LINUX_OS
        typedef poll_socket_watcher default_socket_watcher;
#elif POSIX_OS
        typedef poll_socket_watcher default_socket_watcher;
#elif WINDOWS_OS
        typedef poll_socket_watcher default_socket_watcher;
#else
# error Platform not supported
#endif
    }

    class WSAAsyncSelectWatcher;

    template<typename system_watcher = impl::default_socket_watcher>
    class socket_server {
        std::unordered_map<system_socket_descriptor, socket *> third_party_socket_map;           // Maps third-party descriptors (servers can watch other sockets than just incoming connections), not owned by this class
        std::unordered_map<system_socket_descriptor, std::unique_ptr<socket>> client_socket_map; // Maps descriptors to their client socket objects
        system_watcher watcher;
        bool canceled;

        socket *third_party_socket(system_socket_descriptor native) const {
            const auto it = third_party_socket_map.find(native);
            if (it != third_party_socket_map.end())
                return it->second;

            return nullptr;
        }
        socket *owned_socket(system_socket_descriptor native) const {
            const auto it = client_socket_map.find(native);
            if (it != client_socket_map.end())
                return it->second.get();

            return nullptr;
        }
        socket *get_socket(system_socket_descriptor native) const {
            auto sock = third_party_socket(native);
            if (sock)
                return sock;

            return owned_socket(native);
        }

        void socket_accept_event_occurred(socket *s, socket_watch_flags) {
            std::error_code ec;
            struct sockaddr_storage remote_addr;
            socklen_t remote_addr_len = sizeof(remote_addr);

            do {
#if POSIX_OS | WINDOWS_OS
                const system_socket_descriptor remote = ::accept(s->native(),
                                                                 reinterpret_cast<struct sockaddr *>(&remote_addr),
                                                                 &remote_addr_len);

                if (remote == impl::system_invalid_socket_value) {
                    ec = impl::socket_error();
                    if (impl::socket_would_block(ec))
                        break;

                    s->error(ec);
                    break;
                } else {
                    // Platform-dependent blocking inheritance. On Linux, sockets don't inherit blocking spec, whereas Windows and BSD do.
                    const bool is_blocking =
#if LINUX_OS
                            true;
#else
                            s->is_blocking();
#endif


                    auto p = s->create(remote, socket_state::connected, is_blocking);
                    if (!p)
                        continue;

                    if (!do_socket_init(p.get())) {
                        if (ec) {
                            p->error(ec);
                            error(p.get(), ec);
                            continue;
                        }

                        client_socket_map.insert({remote, std::move(p)});

                        s->do_server_connected(ec);
                        if (ec) {
                            p->error(ec);
                            error(p.get(), ec);
                        }
                    }
                }
#else
# error Platform not supported
#endif
            } while (!s->is_blocking()); // If non-blocking listener socket, then read all available new connections immediately
        }

        void socket_nonaccept_event_occurred(socket *s, socket_watch_flags flags) {
            const system_socket_descriptor desc = s->native(); // Must be called before callbacks. If called after, the user may have closed the socket and erased the descriptor
            const socket_state original_state = s->state();
            std::error_code ec;

            s->did_write = false;

            const bool attempt_read = (flags & WatchRead) || s->async_pending_read();
            const bool attempt_write = flags & WatchWrite;

            if (!s->is_listening()) { // Ignore read/write events on accept()ing socket
                if (attempt_write)
                    s->do_server_write(ec);

                if (attempt_read && !s->is_null())
                    s->do_server_read(ec);
            }

            // Was it a hangup, or socket disconnected in callback? If so, unwatch and delete
            if (((flags & WatchHangup) && !attempt_read) ||                             // Originally disconnected, that's why the event came in
                (s->state() != original_state && s->is_null())) {                       // Socket already was disconnected
                s->do_server_disconnected(ec);
                watcher.unwatch_dead_descriptor(ec, desc);
                third_party_socket_map.erase(desc);
                client_socket_map.erase(desc);
            } else if (s->did_write) {                                                  // Data was queued to send, enable write watching
                update_blocking(s, watcher.modify(ec, desc, WatchAll));
            } else if ((flags & WatchWrite) && !s->did_write && !s->async_pending_write()) { // No data queued and no data sent, disable write watching
                update_blocking(s, watcher.modify(ec, desc, WatchAll & ~WatchWrite));
            }

            if (ec) {
                s->error(ec);
                error(s, ec);
            }
        }

        std::error_code do_socket_init(socket *s) {
            std::error_code ec;

            update_blocking(s, watcher.watch(ec, s->native(), WatchAll));

            if (ec) {
                s->error(ec);
                error(s, ec);
            }

            return ec;
        }

        void update_blocking(socket *s, socket_blocking_adjustment adjustment) {
            switch (adjustment) {
                case socket_blocking_adjustment::blocking:      s->blocking = true; break;
                case socket_blocking_adjustment::nonblocking:   s->blocking = false; break;
                default: break;
            }
        }

    protected:
        virtual void ready_read(socket *, std::error_code &) {}
        virtual void ready_write(socket *, std::error_code &) {}
        virtual void error(socket *, std::error_code ec) { std::cout << ec.message() << std::endl; }

    public:
        socket_server() : canceled(false) {}
        template<typename... Args>
        explicit socket_server(Args&&... args) : watcher(std::forward<Args>(args)...), canceled(false) {}
        virtual ~socket_server() {}

        // Add external socket to be watched by this server (any socket type works)
        void serve_socket(socket *s) {
            if (s->is_null())
                throw std::logic_error("Cannot serve a null socket");

            if (!do_socket_init(s))
                third_party_socket_map[s->native()] = s;
        }

        // Run this server, constantly polling for updates
        // Disabled for WSAAsyncSelect watchers, use message_received() in a message handler instead
#if WINDOWS_OS
        template<typename W = system_watcher, typename std::enable_if<!std::is_same<W, WSAAsyncSelectWatcher>::value, bool>::type = true>
#endif
        void run() {
            canceled = false;
            while (!canceled && third_party_socket_map.size()) {
                poll();
                std::this_thread::yield();
            }
        }

        // Cancel a running server
        void cancel() { canceled = true; }

        // Polls the set of socket descriptors for changes
        // listen(), poll(), and run() are the only non-reentrant function in this class
        // Disabled for WSAAsyncSelect watchers, use message_received() in a message handler instead
#if WINDOWS_OS
        template<typename W = system_watcher, typename std::enable_if<!std::is_same<W, WSAAsyncSelectWatcher>::value, bool>::type = true>
#endif
        void poll(socket_timeout timeout = socket_timeout::infinite()) {
            std::error_code ec;

            watcher.poll(ec, [&](system_socket_descriptor desc, socket_watch_flags flags) {
                auto socket = get_socket(desc);
                if (!socket)
                    return; // Not being served by this server?

                if (socket->is_listening()) { // Listening socket is ready to read
                    socket_accept_event_occurred(socket, flags);
                } else {
                    socket_nonaccept_event_occurred(socket, flags);
                }
            }, timeout);

            if (ec) {
                error(nullptr, ec);

                cancel();
            }
        }

#if WINDOWS_OS
        // For WSAAsyncSelectWatcher, a message was received for a socket being watched on this server
        template<typename W = system_watcher, typename std::enable_if<std::is_same<W, WSAAsyncSelectWatcher>::value, bool>::type = true>
        void message_received(WPARAM wParam, LPARAM lParam) {
            const system_socket_descriptor desc = wParam;
            const WORD event = WSAGETSELECTEVENT(lParam);
            const WORD error = WSAGETSELECTERROR(lParam);

            socket *sock = get_socket(desc);
            if (!sock)
                return; // Not being served by this server?

            // If an error, signal it on the socket
            if (error) {
                const auto ec = std::error_code(error, win32_category());
                sock->error(ec);
                this->error(sock, ec);
                return;
            }

            const socket_watch_flags flags = WSAAsyncSelectWatcher::watch_flags_from_kernel_flags(event);

            // Otherwise signal the event on the socket
            if (event & FD_ACCEPT) {
                socket_accept_event_occurred(sock, flags);
            } else { // Not a listener socket
                socket_nonaccept_event_occurred(sock, flags);
            }
        }
#endif
    };
}

#endif // SKATE_SERVER_H
