#ifndef POLL_H
#define POLL_H

#include <chrono>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cerrno>

#include "environment.h"

#if POSIX_OS
#include <sys/poll.h>
#include <unistd.h>

namespace Skate {
    class Poll
    {
        mutable std::vector<pollfd> fds;    // Set of file descriptors and events to watch. Sorted if fds_sorted is true, unsorted if fds_sorted is false. Mutable only for sorting purposes.
        mutable bool fds_sorted;            // If false, fds needs to be sorted before searching.

        static bool pollfd_compare(const pollfd &lhs, const pollfd &rhs) {
            return lhs.fd < rhs.fd;
        }

        inline void sort_as_needed() const {
            if (!fds_sorted) {
                std::sort(fds.begin(), fds.end(), pollfd_compare);
                fds_sorted = true;
            }
        }

    public:
        using WatchFlags = uint8_t;

        static constexpr WatchFlags WatchRead   = 1 << 0;
        static constexpr WatchFlags WatchWrite  = 1 << 1;
        static constexpr WatchFlags WatchExcept = 1 << 2;
        static constexpr WatchFlags WatchError  = 1 << 3;
        static constexpr WatchFlags WatchHangup = 1 << 4;
        static constexpr WatchFlags WatchInvalid= 1 << 5;
        static constexpr WatchFlags WatchAll = WatchRead | WatchWrite | WatchExcept | WatchError | WatchHangup | WatchInvalid;

    private:
        static WatchFlags watch_flags_from_kernel_flags(short kernel_flags) {
            WatchFlags watch_flags = 0;

            watch_flags |= kernel_flags & POLLIN? WatchRead: 0;
            watch_flags |= kernel_flags & POLLOUT? WatchWrite: 0;
            watch_flags |= kernel_flags & POLLPRI? WatchExcept: 0;
            watch_flags |= kernel_flags & POLLERR? WatchError: 0;
            watch_flags |= kernel_flags & POLLHUP? WatchHangup: 0;
            watch_flags |= kernel_flags & POLLNVAL? WatchInvalid: 0;

            return watch_flags;
        }

        static short kernel_flags_from_watch_flags(WatchFlags watch_flags) {
            short kernel_flags = 0;

            kernel_flags |= watch_flags & WatchRead? POLLIN: 0;
            kernel_flags |= watch_flags & WatchWrite? POLLOUT: 0;
            kernel_flags |= watch_flags & WatchExcept? POLLPRI: 0;

            return kernel_flags;
        }

    public:
        Poll() : fds_sorted(true) {}

        // Returns which watch types are being watched for the given file descriptor,
        // or 0 if the descriptor is not being watched.
        WatchFlags watching(FileDescriptor fd) const {
            sort_as_needed();

            pollfd desc;
            desc.fd = fd;
            const auto it = std::lower_bound(fds.begin(), fds.end(), desc, pollfd_compare);
            if (it == fds.end() || it->fd != fd)
                return 0;

            return watch_flags_from_kernel_flags(it->events);
        }

        // Adds a file descriptor to the set to be watched with the specified watch types
        // The descriptor must not already exist in the set, or the behavior is undefined
        void watch(FileDescriptor fd, WatchFlags watch_type = WatchRead) {
            pollfd desc;

            desc.fd = fd;
            desc.events = kernel_flags_from_watch_flags(watch_type);

            fds.push_back(desc);
            fds_sorted = false;
        }

        // Removes a file descriptor from all watch types
        // If the descriptor is not in the set, nothing happens
        void unwatch(FileDescriptor fd) {
            sort_as_needed();

            pollfd desc;
            desc.fd = fd;
            const auto it = std::lower_bound(fds.begin(), fds.end(), desc, pollfd_compare);
            if (it == fds.end() || it->fd != fd)
                return;

            fds.erase(it);
        }

        // Clears the set and removes all watched descriptors
        void clear() {
            fds.clear();
            fds_sorted = true;
        }

        // Closes the specified file descriptor, unwatching it from all watch types
        // Returns 0 on success, or a system error if an error occurs
        int close(FileDescriptor fd) {
            unwatch(fd);
            return ::close(fd);
        }

        // Closes all file descriptors in the set, unwatching them from all watch types
        void close_all() {
            for (const pollfd &desc: fds)
                close(desc.fd);

            fds.clear();
            fds_sorted = true;
        }

        // Runs poll() on this set, with a callback function `void (FileDescriptor, WatchFlags)`
        // The callback function is called with each file descriptor that has a change, as well as what status the descriptor is in (in WatchFlags)
        // Returns 0 on success, or a system error if an error occurs
        template<typename Fn>
        int poll(Fn fn) const {
            int ready = ::poll(fds.data(), fds.size(), -1);
            if (ready < 0)
                return errno;

            for (const pollfd &desc: fds) {
                WatchFlags watch_flags = watch_flags_from_kernel_flags(desc.revents);

                if (watch_flags)
                    fn(desc.fd, watch_flags);
            }

            return 0;
        }

        template<typename Fn>
        int poll(Fn fn, std::chrono::milliseconds ms) {
            if (ms.count() > INT_MAX)
                return EINVAL;

            int ready = ::poll(fds.data(), fds.size(), ms.count());
            if (ready < 0)
                return errno;

            for (const pollfd &desc: fds) {
                WatchFlags watch_flags = watch_flags_from_kernel_flags(desc.revents);

                if (watch_flags)
                    fn(desc.fd, watch_flags);
            }

            return 0;
        }
    };
}
#endif

#endif // POLL_H
