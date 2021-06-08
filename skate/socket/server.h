#ifndef SKATE_SERVER_H
#define SKATE_SERVER_H

#include "socket.h"

#include "select.h"
#include "poll.h"
#include "epoll.h"
#include "wsaasyncselect.h"

#include <unordered_map>
#include <utility>
#include <thread>

#include <type_traits>

namespace Skate {
    namespace impl {
#if LINUX_OS
        typedef EPoll DefaultSystemWatcher;
#elif POSIX_OS
        typedef Poll DefaultSystemWatcher;
#elif WINDOWS_OS
        typedef Poll DefaultSystemWatcher;
#else
# error Platform not supported
#endif
    }

    template<typename SystemWatcher = impl::DefaultSystemWatcher>
    class SocketServer {
    public:
        // Called when a new socket descriptor was accept()ed from the listener socket
        //
        // If not specified, a default function will be used
        typedef std::function<void (SocketDescriptor)> NewNativeConnectionFunction;
        // Arguments are:
        //    `socket`, the socket that was just created
        //
        // A return value of 0 disconnects the socket, requesting that it not be allowed to connect
        // Any other return value will set the initial "currently watching" flags of the socket
        typedef std::function<WatchFlags (Socket *)> NewConnectionFunction;
        // Arguments are:
        //    `socket`, the socket being watched
        //  `watching`, the flags currently being watched
        //     `event`, the flags specifying which event just occurred/is available
        //
        // A return value equal to `watching` does nothing
        // A return value of 0 disconnects the socket, requesting watching of nothing
        // Any other return value will modify the "currently watching" flags of the socket
        typedef std::function<WatchFlags (Socket *, WatchFlags, WatchFlags)> WatchFunction;

        enum SystemWatcherSocketPolicy {
            SocketAlwaysBlocking,       // All sockets with this system watcher are blocking (force listener to blocking, child sockets inherit)
            SocketAlwaysNonBlocking,    // All sockets with this system watcher are nonblocking (force listener to nonblocking, child sockets inherit, e.g. WSAAsyncSelect)
            SocketInheritOnAccept,      // Child socket inherits properties from listener socket on accept(), don't change listener socket settings (e.g. BSD)
            SocketBlockingOnAccept,     // Child socket is always blocking after accept(), don't change listener socket settings (e.g. Linux)
        };

#if WINDOWS_OS
        static constexpr SystemWatcherSocketPolicy socket_policy = std::is_same<SystemWatcher, WSAAsyncSelectWatcher>::value? SocketAlwaysNonBlocking: SocketInheritOnAccept;
#elif LINUX_OS
        static constexpr SystemWatcherSocketPolicy socket_policy = SocketBlockingOnAccept;
#else
#error Platform not supported
#endif

    protected:
        struct SocketInfo {
            WatchFlags currently_watching;
            std::unique_ptr<Socket> socket;
        };

        std::unordered_map<SocketDescriptor, SocketInfo> sockets;
        SystemWatcher system_watcher;

        WatchFunction watch_callback;
        NewNativeConnectionFunction new_native_connection_callback;
        NewConnectionFunction new_connection_callback;

        Socket *listener; // Not owned by this class

        // Overridable factory that creates a socket object for the client, and whether it's already blocking or nonblocking
        virtual std::unique_ptr<Socket> socket_factory(SocketDescriptor client, bool nonblocking) {
            return std::unique_ptr<Socket>(new Socket(client, nonblocking));
        }

        // Default implementation of on_new_native_connection just inserts the connection as a watched socket of this server instance
        void new_native_connection(SocketDescriptor client) {
            bool blocking = false;

            // Create new socket device with the blocking specifier. This is not configurable, it's set by the OS and which system watcher is used
            switch (socket_policy) {
                case SocketBlockingOnAccept:    /* fallthrough */
                case SocketAlwaysBlocking:      blocking = true;                    break;
                case SocketAlwaysNonBlocking:   blocking = false;                   break;
                case SocketInheritOnAccept:     blocking = listener->is_blocking(); break;
            }

            // Create new socket from socket factory
            std::unique_ptr<Socket> ptr;

            try {
                ptr = std::move(std::unique_ptr<Socket>(socket_factory(client, !blocking)));
            } catch (const std::bad_alloc &) {
                Socket::close_socket(client);
                return;
            }

            // Assume watch all if no "new connection" callback is set
            const WatchFlags flags = new_connection_callback? new_connection_callback(ptr.get()): WatchAll;

            // User can veto the connection by returning 0 for watch flags
            if (flags == 0)
                return;

            // Or the attempt to watch the socket descriptor failed
            if (system_watcher.try_watch(client, flags))
                sockets[client] = {flags, std::move(ptr)};
        }

        // Called when a listener socket had a readiness event occur
        void socket_accept_event_occurred(Socket *socket, WatchFlags) {
            struct sockaddr_storage remote_addr;
            socklen_t remote_addr_len = sizeof(remote_addr);

            do {
#if POSIX_OS | WINDOWS_OS
                const SocketDescriptor remote = ::accept(socket->native(),
                                                         reinterpret_cast<struct sockaddr *>(&remote_addr),
                                                         &remote_addr_len);

                if (remote == Socket::invalid_socket) {
                    const int err = Socket::socket_error();
                    if (Socket::socket_would_block(err))
                        break;

                    socket->handle_error(err);
                    break;
                }
                else
                    new_native_connection_callback(remote);
#else
# error Platform not supported
#endif
            } while (!socket->is_blocking()); // If non-blocking listener socket, then read all available new connections immediately
        }

        // Called when a non-listener socket had (at least) one readiness event occur
        //
        // The library user can modify what the socket is watched for by returning new values for flags
        //
        // `currently_watching` is the flags currently being watched on the socket (e.g. watch for read, watch for write, etc.)
        // `event` is the flags that were signalled on the socket as happening (e.g. ready to read, ready to write, etc.)
        // `event` may have multiple flags set
        void socket_nonaccept_event_occurred(Socket *socket, WatchFlags currently_watching, WatchFlags event) {
            const SocketDescriptor desc = socket->native(); // Must be called before watch_callback. If called after, the user may have closed the socket and erased the descriptor
            const Socket::State original_state = socket->state();

            const WatchFlags new_flags = watch_callback(socket, currently_watching, event);

            // Was it a hangup, or socket disconnected in callback? If so, unwatch and delete
            if ((event & WatchHangup) ||                                            // Originally disconnected, that's why the event came in
                (new_flags == 0) ||                                                 // User wants to disconnect
                (socket->state() != original_state && socket->is_unconnected())) {  // Socket already was disconnected
                system_watcher.unwatch_dead_descriptor(desc);
                sockets.erase(desc);
            } else if (new_flags != currently_watching) { // If library user changed the watch flags for the descriptor, update the watcher
                system_watcher.modify(desc, new_flags);
            }
        }

    public:
        SocketServer() : listener(nullptr) {}
        template<typename... Args>
        explicit SocketServer(Args&&... args) : system_watcher(std::forward<Args>(args)...) {}

        SystemWatcher &watcher() {return system_watcher;}

        void on_new_native_connection(NewNativeConnectionFunction fn) {new_native_connection_callback = fn;}
        void on_new_connection(NewConnectionFunction fn) {new_connection_callback = fn;}
        void on_watch(WatchFunction fn) {watch_callback = fn;}

        void listen(Socket &socket, NewNativeConnectionFunction fn) {
            on_new_native_connection(fn);
            listen(socket);
        }
        void listen(Socket &socket, NewConnectionFunction fn) {
            on_new_connection(fn);
            listen(socket);
        }
        void listen(Socket &socket, NewConnectionFunction cfn, WatchFunction wfn) {
            on_new_connection(cfn);
            on_watch(wfn);
            listen(socket);
        }
        void listen(Socket &socket, WatchFunction fn) {
            on_watch(fn);
            listen(socket);
        }
        void listen(Socket &socket) {
            if (socket.is_bound())
                socket.listen();

            if (!socket.is_listening())
                throw std::logic_error("SocketServer can only use listen() if socket is bound to an address");

            if (!new_native_connection_callback &&
                !watch_callback)
                throw std::logic_error("SocketServer can only use listen() if either on_new_native_connection() and/or on_watch() have been called");

            if (!new_native_connection_callback) {
                // Set up default native connection callback (the virtual override in this class) if none was set up yet
                on_new_native_connection([this](SocketDescriptor client) {
                    new_native_connection(client);
                });
            }

            if (socket_policy == SocketAlwaysBlocking)
                socket.set_blocking(true);
            else if (socket_policy == SocketAlwaysNonBlocking)
                socket.set_blocking(false);

            system_watcher.clear();
            system_watcher.watch(socket.native(), WatchRead);
            listener = &socket;
        }

        void run() {
            if (!listener)
                throw std::logic_error("SocketServer requires that listen() be called before run()");

            while (1) {
                poll();
                std::this_thread::yield();
            }
        }

        // Polls the set of socket descriptors for changes
        // listen(), poll(), and run() are the only non-reentrant function in this class
        void poll() {
            int err;

            do {
                err = system_watcher.poll([&](SocketDescriptor desc, WatchFlags flags) {
                    if (desc == listener->native()) { // Listening socket is ready to read
                        socket_accept_event_occurred(listener, flags);
                    } else { // Some other socket had an event, trigger the user's callback
                        const auto it = sockets.find(desc);
                        if (it == sockets.end())
                            return; // Not being served by this server?

                        socket_nonaccept_event_occurred(it->second.socket.get(), it->second.currently_watching, flags);
                    }
                });
            } while (listener->handle_error(err));
        }

#if WINDOWS_OS
        // For WSAAsyncSelectWatcher, a message was received for a socket being watched on this server
        template<typename W = SystemWatcher, typename std::enable_if<std::is_same<W, WSAAsyncSelectWatcher>::value, bool>::type = true>
        void message_received(WPARAM wParam, LPARAM lParam) {
            const SocketDescriptor desc = wParam;
            const WORD event = WSAGETSELECTEVENT(lParam);
            const WORD error = WSAGETSELECTERROR(lParam);

            Socket *socket;
            WatchFlags currently_watching = 0;

            if (desc == listener->native()) {
                socket = listener;
            } else {
                // Find socket that had event by its descriptor
                const auto it = sockets.find(desc);
                if (it == sockets.end())
                    return; // Not being served by this server?

                socket = it->second.socket.get();
                currently_watching = it->second.currently_watching;
            }

            // If an error, signal it on the socket
            if (error)
                socket->handle_error(error);

            const WatchFlags flags = WSAAsyncSelectWatcher::watch_flags_from_kernel_flags(event);

            // Otherwise signal the event on the socket
            if (event & FD_ACCEPT) {
                socket_accept_event_occurred(socket, flags);
            } else { // Not a listener socket
                socket_nonaccept_event_occurred(socket, currently_watching, flags);
            }
        }
#endif
    };
}

#endif // SKATE_SERVER_H
