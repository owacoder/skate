#ifndef SKATE_EPOLL_H
#define SKATE_EPOLL_H

#include "../system/includes.h"
#include "common.h"

#if LINUX_OS
# include <sys/epoll.h>

namespace skate {
    class EPoll : public SocketWatcher {
        int queue;

    public:
        static WatchFlags watch_flags_from_kernel_flags(uint32_t kernel_flags) noexcept {
            WatchFlags watch_flags = 0;

            watch_flags |= kernel_flags & EPOLLIN? WatchRead: 0;
            watch_flags |= kernel_flags & EPOLLOUT? WatchWrite: 0;
            watch_flags |= kernel_flags & EPOLLPRI? WatchExcept: 0;
            watch_flags |= kernel_flags & EPOLLERR? WatchError: 0;
            watch_flags |= kernel_flags & EPOLLHUP? WatchHangup: 0;

            return watch_flags;
        }

        static uint32_t kernel_flags_from_watch_flags(WatchFlags watch_flags) noexcept {
            uint32_t kernel_flags = 0;

            kernel_flags |= watch_flags & WatchRead? static_cast<uint32_t>(EPOLLIN): 0;
            kernel_flags |= watch_flags & WatchWrite? static_cast<uint32_t>(EPOLLOUT): 0;
            kernel_flags |= watch_flags & WatchExcept? static_cast<uint32_t>(EPOLLPRI): 0;
            kernel_flags |= watch_flags & WatchError? static_cast<uint32_t>(EPOLLERR): 0;
            kernel_flags |= watch_flags & WatchHangup? static_cast<uint32_t>(EPOLLHUP): 0;

            return kernel_flags;
        }

        EPoll() : queue(::epoll_create1(0)) {
            if (queue == Socket::invalid_socket)
                throw std::system_error(errno, std::system_category());
        }
        virtual ~EPoll() { close(queue); }

        virtual WatchFlags watching(SocketDescriptor) const {
            // TODO: no way to determine if kernel has socket in set already
            return 0;
        }

        void watch(SocketDescriptor socket, WatchFlags watch_type = WatchRead) {
            if (!try_watch(socket, watch_type))
                throw std::system_error(errno, std::system_category());
        }

        bool try_watch(SocketDescriptor socket, WatchFlags watch_type = WatchRead) {
            struct epoll_event ev;

            ev.data.fd = socket;
            ev.events = kernel_flags_from_watch_flags(watch_type);

            return ::epoll_ctl(queue, EPOLL_CTL_ADD, socket, &ev) == 0;
        }

        void modify(SocketDescriptor socket, WatchFlags new_watch_type) {
            struct epoll_event ev;

            ev.data.fd = socket;
            ev.events = kernel_flags_from_watch_flags(new_watch_type);

            if (::epoll_ctl(queue, EPOLL_CTL_MOD, socket, &ev) < 0)
                throw std::system_error(errno, std::system_category());
        }

        void unwatch(SocketDescriptor socket) {
            struct epoll_event ev;
            if (::epoll_ctl(queue, EPOLL_CTL_DEL, socket, &ev) < 0 && errno != EBADF)
                throw std::system_error(errno, std::system_category());
        }
        void unwatch_dead_descriptor(SocketDescriptor) {
            /* Do nothing as kernel removes descriptor from epoll() set when close() is called */
        }

        void clear() {
            if (close(queue) || (queue = ::epoll_create1(0)) == Socket::invalid_socket)
                throw std::system_error(errno, std::system_category());
        }

        int poll(NativeWatchFunction fn) {
            struct epoll_event events[1024];

            const int ready = ::epoll_wait(queue, events, sizeof(events)/sizeof(*events), -1);
            if (ready < 0)
                return errno;

            for (int i = 0; i < ready; ++i) {
                fn(events[i].data.fd, watch_flags_from_kernel_flags(events[i].events));
            }

            return 0;
        }

        int poll(NativeWatchFunction fn, std::chrono::microseconds timeout) {
            if (timeout.count() > INT_MAX)
                return EINVAL;

            struct epoll_event events[1024];

            const int ready = ::epoll_wait(queue, events, sizeof(events)/sizeof(*events), timeout.count() / 1000);
            if (ready < 0)
                return errno;

            for (const auto &ev: events) {
                fn(ev.data.fd, watch_flags_from_kernel_flags(ev.events));
            }

            return ready? 0: ErrorTimedOut;
        }
    };
}
#endif

#endif // SKATE_EPOLL_H
