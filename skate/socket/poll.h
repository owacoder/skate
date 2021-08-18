/**
 *
 * Copyright (c) Oliver Adams, 2021.
 *
 * Licensed under Apache 2.0 license
 *
 */

#ifndef SKATE_POLL_H
#define SKATE_POLL_H

#include <chrono>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cerrno>

#include "../system/includes.h"
#include "common.h"

#if POSIX_OS
#include <sys/poll.h>

namespace skate {
    class poll_socket_watcher : public socket_watcher {
        std::vector<pollfd> fds;

    public:
        static socket_watch_flags watch_flags_from_kernel_flags(short kernel_flags) noexcept {
            socket_watch_flags watch_flags = 0;

            watch_flags |= kernel_flags & POLLIN? WatchRead: 0;
            watch_flags |= kernel_flags & POLLOUT? WatchWrite: 0;
            watch_flags |= kernel_flags & POLLPRI? WatchExcept: 0;
            watch_flags |= kernel_flags & POLLERR? WatchError: 0;
            watch_flags |= kernel_flags & POLLHUP? WatchHangup: 0;
            watch_flags |= kernel_flags & POLLNVAL? WatchInvalid: 0;

            return watch_flags;
        }

        static short kernel_flags_from_watch_flags(socket_watch_flags watch_flags) noexcept {
            short kernel_flags = 0;

            kernel_flags |= watch_flags & WatchRead? POLLIN: 0;
            kernel_flags |= watch_flags & WatchWrite? POLLOUT: 0;
            kernel_flags |= watch_flags & WatchExcept? POLLPRI: 0;

            return kernel_flags;
        }

        poll_socket_watcher() {}
        virtual ~poll_socket_watcher() {}

        // Returns which watch types are being watched for the given file descriptor,
        // or 0 if the descriptor is not being watched.
        virtual socket_watch_flags watching(system_socket_descriptor fd) const override {
            const auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it == fds.end())
                return 0;

            return watch_flags_from_kernel_flags(it->events);
        }

        // Adds a file descriptor to the set to be watched with the specified watch types
        // The descriptor must not already exist in the set, or the behavior is undefined
        virtual socket_blocking_adjustment watch(std::error_code &ec, system_socket_descriptor fd, socket_watch_flags watch_type) override {
            pollfd desc;

            desc.fd = fd;
            desc.events = kernel_flags_from_watch_flags(watch_type);

            ec.clear();

            fds.push_back(desc);

            return socket_blocking_adjustment::unchanged;
        }

        // Modifies the watch flags for a descriptor in the set
        // If the descriptor is not in the set, nothing happens
        virtual socket_blocking_adjustment modify(std::error_code &ec, system_socket_descriptor fd, socket_watch_flags new_watch_type) override {
            ec.clear();

            auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it != fds.end())
                it->events = kernel_flags_from_watch_flags(new_watch_type);

            return socket_blocking_adjustment::unchanged;
        }

        // Removes a file descriptor from all watch types
        // If the descriptor is not in the set, nothing happens
        virtual socket_blocking_adjustment unwatch(std::error_code &ec, system_socket_descriptor fd) override {
            ec.clear();

            const auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it != fds.end())
                fds.erase(it);

            return socket_blocking_adjustment::unchanged;
        }

        // Clears the set and removes all watched descriptors
        virtual void clear(std::error_code &ec) override {
            ec.clear();
            fds.clear();
        }

        // Runs poll() on this set, with a callback function `void (system_socket_descriptor, socket_watch_flags)`
        // The callback function is called with each file descriptor that has a change, as well as what status the descriptor is in (in socket_watch_flags)
        virtual void poll(std::error_code &ec, native_watch_function fn, socket_timeout timeout) override {
            if (!timeout.is_infinite() && timeout.timeout().count() / 1000 > INT_MAX) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            }

            const int ready = ::poll(fds.data(), fds.size(), timeout.is_infinite()? -1: static_cast<int>(timeout.timeout().count() / 1000));
            if (ready < 0) {
                ec = impl::socket_error();
            } else if (ready == 0) {
                ec = std::make_error_code(std::errc::timed_out);
            } else {
                ec.clear();

                // Make copy of triggered sockets because all watch()/unwatch() functions are reentrant and could modify fds
                std::vector<struct pollfd> fds_triggered;
                for (const pollfd &desc: fds) {
                    if (desc.revents)
                        fds_triggered.push_back(desc);
                }

                // Now iterate through the copies of triggered sockets
                for (const pollfd &desc: fds_triggered) {
                    fn(desc.fd, watch_flags_from_kernel_flags(desc.revents));
                }
            }
        }
    };
}
#elif WINDOWS_OS // End of POSIX_OS
namespace skate {
    class poll_socket_watcher : public socket_watcher {
        std::vector<struct pollfd> fds;

    public:
        static socket_watch_flags watch_flags_from_kernel_flags(short kernel_flags) noexcept {
            socket_watch_flags watch_flags = 0;

            watch_flags |= kernel_flags & POLLIN? WatchRead: 0;
            watch_flags |= kernel_flags & POLLOUT? WatchWrite: 0;
            watch_flags |= kernel_flags & POLLPRI? WatchExcept: 0;
            watch_flags |= kernel_flags & POLLERR? WatchError: 0;
            watch_flags |= kernel_flags & POLLHUP? WatchHangup: 0;
            watch_flags |= kernel_flags & POLLNVAL? WatchInvalid: 0;

            return watch_flags;
        }

        static short kernel_flags_from_watch_flags(socket_watch_flags watch_flags) noexcept {
            short kernel_flags = 0;

            kernel_flags |= watch_flags & WatchRead? POLLIN: 0;
            kernel_flags |= watch_flags & WatchWrite? POLLOUT: 0;
            // Can't request POLLPRI on Windows because Windows bombs out with an invalid argument error if requested
            // See https://stackoverflow.com/questions/55524397/why-when-i-add-pollhup-as-event-wsapoll-returns-error-invalid-arguments

            return kernel_flags;
        }

        poll_socket_watcher() {}
        virtual ~poll_socket_watcher() {}

        // Returns which watch types are being watched for the given file descriptor,
        // or 0 if the descriptor is not being watched.
        virtual socket_watch_flags watching(system_socket_descriptor fd) const override {
            const auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it == fds.end())
                return 0;

            return watch_flags_from_kernel_flags(it->events);
        }

        // Adds a file descriptor to the set to be watched with the specified watch types
        // The descriptor must not already exist in the set, or the behavior is undefined
        virtual socket_blocking_adjustment watch(std::error_code &ec, system_socket_descriptor fd, socket_watch_flags watch_type) override {
            pollfd desc;

            desc.fd = fd;
            desc.events = kernel_flags_from_watch_flags(watch_type);

            ec.clear();

            fds.push_back(desc);

            return socket_blocking_adjustment::unchanged;
        }

        // Modifies the watch flags for a descriptor in the set
        // If the descriptor is not in the set, nothing happens
        virtual socket_blocking_adjustment modify(std::error_code &ec, system_socket_descriptor fd, socket_watch_flags new_watch_type) override {
            ec.clear();

            auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it != fds.end())
                it->events = kernel_flags_from_watch_flags(new_watch_type);

            return socket_blocking_adjustment::unchanged;
        }

        // Removes a file descriptor from all watch types
        // If the descriptor is not in the set, nothing happens
        virtual socket_blocking_adjustment unwatch(std::error_code &ec, system_socket_descriptor fd) override {
            ec.clear();

            auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it != fds.end()) {
                const size_t last_element = fds.size() - 1;
                std::swap(fds[last_element], *it); // Swap with last element since the order is irrelevant
                fds.resize(last_element);
            }

            return socket_blocking_adjustment::unchanged;
        }

        // Clears the set and removes all watched descriptors
        virtual void clear(std::error_code &ec) override {
            ec.clear();
            fds.clear();
        }

        // Runs poll() on this set, with a callback function `void (system_socket_descriptor, socket_watch_flags)`
        // The callback function is called with each file descriptor that has a change, as well as what status the descriptor is in (in socket_watch_flags)
        virtual void poll(std::error_code &ec, socket_watcher::native_watch_function fn, socket_timeout timeout) override {
            if (!timeout.is_infinite() && timeout.timeout().count() / 1000 > INT_MAX) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            } else if (fds.empty()) { // Nothing to poll
                ec.clear();
                return;
            }

            const int ready = ::WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), timeout.is_infinite()? -1: static_cast<int>(timeout.timeout().count() / 1000));
            if (ready < 0) {
                ec = impl::socket_error();
            } else if (ready == 0) {
                ec = std::make_error_code(std::errc::timed_out);
            } else {
                ec.clear();

                // Make copy of triggered sockets because all watch()/unwatch() functions are reentrant and could modify fds
                std::vector<struct pollfd> fds_triggered;
                for (const pollfd &desc: fds) {
                    if (desc.revents)
                        fds_triggered.push_back(desc);
                }

                // Now iterate through the copies of triggered sockets
                for (const pollfd &desc: fds_triggered) {
                    fn(desc.fd, watch_flags_from_kernel_flags(desc.revents));
                }
            }
        }
    };
}
#endif

#endif // SKATE_POLL_H
