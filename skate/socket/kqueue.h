/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_KQUEUE_H
#define SKATE_KQUEUE_H

#include "../system/includes.h"
#include "common.h"

// TODO: kqueue() implementation not working or tested yet
#if BSD_OS | MAC_OS
# include <sys/event.h>

namespace skate {
    class KQueue : public SocketWatcher {
        std::vector<struct kevent> changes;
        system_socket_descriptor queue;

        static socket_watch_flags watch_flags_from_kernel_flags(uint32_t kernel_flags) noexcept {
            socket_watch_flags watch_flags = 0;

            watch_flags |= kernel_flags & EPOLLIN? WatchRead: 0;
            watch_flags |= kernel_flags & EPOLLOUT? WatchWrite: 0;
            watch_flags |= kernel_flags & EPOLLPRI? WatchExcept: 0;
            watch_flags |= kernel_flags & EPOLLERR? WatchError: 0;
            watch_flags |= kernel_flags & EPOLLHUP? WatchHangup: 0;

            return watch_flags;
        }

        static uint32_t kernel_flags_from_watch_flags(socket_watch_flags watch_flags) noexcept {
            uint32_t kernel_flags = 0;

            kernel_flags |= watch_flags & WatchRead? static_cast<uint32_t>(EPOLLIN): 0;
            kernel_flags |= watch_flags & WatchWrite? static_cast<uint32_t>(EPOLLOUT): 0;
            kernel_flags |= watch_flags & WatchExcept? static_cast<uint32_t>(EPOLLPRI): 0;
            kernel_flags |= watch_flags & WatchError? static_cast<uint32_t>(EPOLLERR): 0;
            kernel_flags |= watch_flags & WatchHangup? static_cast<uint32_t>(EPOLLHUP): 0;

            return kernel_flags;
        }

    public:
        KQueue() : queue(::kqueue()) {
            if (queue == skate::Socket::invalid_socket)
                throw std::runtime_error(system_error_string(errno).to_utf8());
        }
        virtual ~KQueue() { close(queue); }

        virtual socket_watch_flags watching(system_socket_descriptor) const {
            // TODO: no way to determine if kernel has socket in set already
            return 0;
        }

        void watch(system_socket_descriptor socket, socket_watch_flags watch_type = WatchRead) {
            if (!try_watch(socket, watch_type))
                throw std::runtime_error(system_error_string(errno).to_utf8());
        }

        bool try_watch(system_socket_descriptor socket, socket_watch_flags watch_type = WatchRead) {
            struct kevent ev;

            ev.ident = socket;
            ev.filter = kernel_flags_from_watch_flags(watch_type);
            ev.flags = EV_ADD;
            ev.fflags = 0;
            ev.data = 0;
            ev.udata = nullptr;

            changes.push_back(ev);
            return true;
        }

        void modify(system_socket_descriptor socket, socket_watch_flags new_watch_type) {
            watch(socket, new_watch_type);
        }

        void unwatch(system_socket_descriptor socket) {
            struct kevent ev;

            ev.ident = socket;
            ev.flags = EV_DELETE;

            changes.push_back(ev);
        }
        void unwatch_dead_descriptor(system_socket_descriptor) {
            /* Do nothing as kernel removes descriptor from epoll() set when close() is called */
        }

        void clear() {
            if (close(queue) || (queue = ::kqueue()) == Socket::invalid_socket)
                throw std::runtime_error(system_error_string(errno).to_utf8());
        }

        int poll(NativeWatchFunction fn, struct timespec *timeout) {
            struct kevent events[1024];

            const int ready = ::kevent(queue,
                                       changes.data(),
                                       changes.size(),
                                       events,
                                       sizeof(events)/sizeof(*events),
                                       timeout);
            if (ready < 0)
                return errno;

            changes.clear();
            for (int i = 0; i < ready; ++i) {
                fn(events[i].data.fd, watch_flags_from_kernel_flags(events[i].events));
            }

            return 0;
        }

        int poll(NativeWatchFunction fn) {
            return poll(fn, nullptr);
        }
        int poll(NativeWatchFunction fn, std::chrono::microseconds us) {
            struct timespec tm;

            tm.tv_sec = static_cast<long>(us.count() / 1000000);
            tm.tv_nsec = (us.count() % 1000000) * 1000;

            return poll(fn, &tm);
        }
    };
}
#endif

#endif // SKATE_KQUEUE_H
