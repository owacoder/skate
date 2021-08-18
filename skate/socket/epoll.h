/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_EPOLL_H
#define SKATE_EPOLL_H

#include "../system/includes.h"
#include "common.h"

#if LINUX_OS
# include <sys/epoll.h>

namespace skate {
    class epoll_socket_watcher : public socket_watcher {
        int queue;

    public:
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

        epoll_socket_watcher() : queue(::epoll_create1(0)) {}
        virtual ~epoll_socket_watcher() {
            close(queue);
        }

        virtual socket_watch_flags watching(system_socket_descriptor) const override {
            // TODO: no way to determine if kernel has socket in set already
            return 0;
        }

        virtual socket_blocking_adjustment watch(std::error_code &ec, system_socket_descriptor socket, socket_watch_flags watch_type) override {
            struct epoll_event ev;

            ev.data.fd = socket;
            ev.events = kernel_flags_from_watch_flags(watch_type);

            if (::epoll_ctl(queue, EPOLL_CTL_ADD, socket, &ev) != 0)
                ec = impl::socket_error();
            else
                ec.clear();

            return socket_blocking_adjustment::unchanged;
        }

        virtual socket_blocking_adjustment modify(std::error_code &ec, system_socket_descriptor socket, socket_watch_flags new_watch_type) override {
            struct epoll_event ev;

            ev.data.fd = socket;
            ev.events = kernel_flags_from_watch_flags(new_watch_type);

            if (::epoll_ctl(queue, EPOLL_CTL_MOD, socket, &ev) != 0)
                ec = impl::socket_error();
            else
                ec.clear();

            return socket_blocking_adjustment::unchanged;
        }

        virtual socket_blocking_adjustment unwatch(std::error_code &ec, system_socket_descriptor socket) override {
            struct epoll_event ev;

            if (::epoll_ctl(queue, EPOLL_CTL_DEL, socket, &ev) != 0 && errno != EBADF)
                ec = impl::socket_error();
            else
                ec.clear();

            return socket_blocking_adjustment::unchanged;
        }

        virtual void unwatch_dead_descriptor(std::error_code &ec, system_socket_descriptor) override {
            ec.clear();
            /* Do nothing as kernel removes descriptor from epoll() set when close() is called */
        }

        virtual void clear(std::error_code &ec) override {
            const int err = ::close(queue);
            if (err)
                ec = std::error_code(err, std::system_category());

            queue = ::epoll_create1(0);
            if (queue == impl::system_invalid_socket_value)
                ec = impl::socket_error();
        }

        virtual void poll(std::error_code &ec, native_watch_function fn, socket_timeout timeout) override {
            if (!timeout.is_infinite() && timeout.timeout().count() / 1000 > INT_MAX) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            }

            struct epoll_event events[1024];

            const int ready = ::epoll_wait(queue, events, sizeof(events)/sizeof(*events), timeout.is_infinite()? -1: timeout.timeout().count() / 1000);
            if (ready < 0) {
                ec = impl::socket_error();
            } else if (ready == 0) {
                ec = std::make_error_code(std::errc::timed_out);
            } else {
                ec.clear();

                for (const auto &ev: events) {
                    fn(ev.data.fd, watch_flags_from_kernel_flags(ev.events));
                }
            }
        }
    };
}
#endif

#endif // SKATE_EPOLL_H
