#ifndef SKATE_SOCKET_COMMON_H
#define SKATE_SOCKET_COMMON_H

#include "socket.h"

#include <unordered_map>

namespace skate {
    typedef uint8_t socket_watch_flags;

    static constexpr socket_watch_flags WatchRead    = 1 << 0;
    static constexpr socket_watch_flags WatchWrite   = 1 << 1;
    static constexpr socket_watch_flags WatchExcept  = 1 << 2;
    static constexpr socket_watch_flags WatchError   = 1 << 3;
    static constexpr socket_watch_flags WatchHangup  = 1 << 4;
    static constexpr socket_watch_flags WatchInvalid = 1 << 5;
    static constexpr socket_watch_flags WatchAll     = 0xff;

    class socket_timeout {
        std::chrono::microseconds t;
        bool infinite_;

    public:
        constexpr socket_timeout() : t{}, infinite_(true) {}
        constexpr socket_timeout(std::chrono::microseconds timeout) : t(timeout), infinite_(false) {}

        constexpr static socket_timeout infinite() noexcept { return {}; }

        constexpr bool is_infinite() const noexcept { return infinite_; }
        constexpr std::chrono::microseconds timeout() const noexcept { return t; }
    };

    class socket_watcher {
        socket_watcher(const socket_watcher &) = delete;
        socket_watcher &operator=(const socket_watcher &) = delete;

    protected:
        socket_watcher() {}

        template<typename socket_watcher>
        friend class socket_server;

        typedef std::function<void (system_socket_descriptor, socket_watch_flags)> native_watch_function;

        // Returns which events are currently being watched on the socket
        // Some watcher types (e.g. epoll(), kqueue()) may not have this information and will always return 0 (not watching)
        virtual socket_watch_flags watching(system_socket_descriptor socket) const = 0;

        // Watches a descriptor with the specified watch flags
        virtual void watch(std::error_code &ec, system_socket_descriptor socket, socket_watch_flags watch_type) = 0;

        // Modifies the watch type for a specified socket
        // If the descriptor is not in the set, nothing happens
        virtual void modify(std::error_code &ec, system_socket_descriptor socket, socket_watch_flags new_watch_type) {
            unwatch(ec, socket);
            if (!ec)
                watch(ec, socket, new_watch_type);
        }

        // Unwatches a descriptor that may still be open
        // If the descriptor is not in the set, nothing happens
        virtual void unwatch(std::error_code &ec, system_socket_descriptor socket) = 0;

        // Unwatches a descriptor known to be closed already
        // If the descriptor is not in the set, nothing happens
        virtual void unwatch_dead_descriptor(std::error_code &ec, system_socket_descriptor socket) {
            unwatch(ec, socket);
        }

        // Clears all descriptors from this watcher
        virtual void clear(std::error_code &ec) = 0;

        // Polls the watcher to obtain events that happened, returns system error on failure, 0 on success
        virtual void poll(std::error_code &ec, native_watch_function fn, socket_timeout us) = 0;

    public:
        virtual ~socket_watcher() {}
    };
}

#endif // SKATE_SOCKET_COMMON_H
