#ifndef SKATE_SERVER_H
#define SKATE_SERVER_H

#include "select.h"
#include "poll.h"

namespace Skate {
    template<typename SystemWatcher = Poll>
    class SocketServer {
    public:
        typedef std::function<void (SocketDescriptor)> NewNativeConnectionFunction;
        typedef std::function<void (Socket *)> NewConnectionFunction;
        typedef std::function<void (Socket *, WatchFlags)> WatchFunction;

    protected:
        std::unordered_map<SocketDescriptor, std::unique_ptr<Socket>> sockets;
        SystemWatcher system_watcher;

        WatchFunction watch_callback;
        NewNativeConnectionFunction new_native_connection_callback;
        NewConnectionFunction new_connection_callback;

        Socket *listener; // Not owned by this class

        void new_native_connection(SocketDescriptor client) {
            if (!system_watcher.try_watch(client, WatchAll)) {
                Socket::close_socket(client);
                return;
            }

            Socket *connection = new Socket(client);

            sockets[client].reset(connection);

            if (new_connection_callback)
                new_connection_callback(connection);
        }

        void existing_native_connection(SocketDescriptor client, WatchFlags flags) {
            const auto it = sockets.find(client);
            Socket * const connection = it->second.get();

            // assert(it != sockets.end());
            // assert(watch_callback);
            watch_callback(connection, flags);

            // Was it a hangup, or socket disconnected in callback? If so, unwatch and delete
            if ((flags & WatchHangup) || connection->is_unconnected()) {
                system_watcher.unwatch(client);
                sockets.erase(client);
            }
        }

    public:
        SocketServer() : listener(nullptr) {}
        virtual ~SocketServer() {}

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

            if (!new_native_connection_callback)
                on_new_native_connection(std::bind(&SocketServer<SystemWatcher>::new_native_connection,
                                                               this,
                                                               std::placeholders::_1));

            system_watcher.clear();
            system_watcher.watch(socket.native(), WatchRead);
            listener = &socket;
        }

        void run() {
            if (!listener)
                throw std::logic_error("SocketServer requires that listen() be called before run()");

            while (1)
                poll();
        }

        void poll() {
            int err;

            do {
                err = system_watcher.poll([&](SocketDescriptor desc, WatchFlags flags) {
                    if (desc == listener->native()) { // Listening socket is ready to read
                        struct sockaddr_storage remote_addr;
                        socklen_t remote_addr_len = sizeof(remote_addr);

                        do {
    #if POSIX_OS
                            const SocketDescriptor remote = ::accept(desc,
                                                                     reinterpret_cast<struct sockaddr *>(&remote_addr),
                                                                     &remote_addr_len);

                            if (remote == Socket::invalid_socket) {
                                if (errno == EAGAIN || errno == EWOULDBLOCK)
                                    break;

                                listener->handle_error(errno);
                            }
                            else
                                new_native_connection_callback(remote);
    #endif
                        } while (!listener->is_blocking()); // If non-blocking listener socket, then read all available new connections immediately
                    } else { // Some other socket had an event, trigger the user's callback
                        existing_native_connection(desc, flags);
                    }
                });
            } while (listener->handle_error(err));
        }
    };
}

#endif // SKATE_SERVER_H
