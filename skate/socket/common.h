#ifndef SKATE_SOCKET_COMMON_H
#define SKATE_SOCKET_COMMON_H

#include "socket.h"

#include <unordered_map>

namespace skate {
    using WatchFlags = uint8_t;

    static constexpr WatchFlags WatchRead    = 1 << 0;
    static constexpr WatchFlags WatchWrite   = 1 << 1;
    static constexpr WatchFlags WatchExcept  = 1 << 2;
    static constexpr WatchFlags WatchError   = 1 << 3;
    static constexpr WatchFlags WatchHangup  = 1 << 4;
    static constexpr WatchFlags WatchInvalid = 1 << 5;
    static constexpr WatchFlags WatchAll     = 0xff;

    template<typename SocketWatcher>
    class SocketServer;

    class SocketWatcher {
        SocketWatcher(const SocketWatcher &) = delete;
        SocketWatcher &operator=(const SocketWatcher &) = delete;

    protected:
        SocketWatcher() {}

        template<typename SocketWatcher>
        friend class SocketServer;

        typedef std::function<void (SocketDescriptor, WatchFlags)> NativeWatchFunction;

        // Returns which events are currently being watched on the socket
        // Some watcher types (e.g. epoll(), kqueue()) may not have this information and will always return 0 (not watching)
        virtual WatchFlags watching(SocketDescriptor socket) const = 0;

        // Watches a descriptor with the specified watch flags
        virtual void watch(SocketDescriptor socket, WatchFlags watch_type) = 0;

        // Attempts to watch a descriptor with the specified watch flags, returns false if not possible
        virtual bool try_watch(SocketDescriptor socket, WatchFlags watch_type) = 0;

        // Modifies the watch type for a specified socket
        // If the descriptor is not in the set, nothing happens
        virtual void modify(SocketDescriptor socket, WatchFlags new_watch_type) {
            unwatch(socket);
            watch(socket, new_watch_type);
        }

        // Unwatches a descriptor that may still be open
        // If the descriptor is not in the set, nothing happens
        virtual void unwatch(SocketDescriptor socket) = 0;

        // Unwatches a descriptor known to be closed already
        // If the descriptor is not in the set, nothing happens
        virtual void unwatch_dead_descriptor(SocketDescriptor socket) {unwatch(socket);}

        // Clears all descriptors from this watcher
        virtual void clear() = 0;

        // Polls the watcher to obtain events that happened, returns system error on failure, 0 on success
        // The first variant is infinite timeout, the second is timeout specified in microseconds
        virtual int poll(NativeWatchFunction fn) = 0;
        virtual int poll(NativeWatchFunction fn, std::chrono::microseconds us) = 0;

    public:
        virtual ~SocketWatcher() {}
    };
}

#endif // SKATE_SOCKET_COMMON_H
