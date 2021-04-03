#ifndef POLL_H
#define POLL_H

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
        Poll() : fds_sorted(false) {}

        WatchFlags watching(FileDescriptor fd) const {
            sort_as_needed();

            pollfd desc;
            desc.fd = fd;
            const auto it = std::lower_bound(fds.begin(), fds.end(), desc, pollfd_compare);
            if (it == fds.end() || it->fd != fd)
                return 0;

            return watch_flags_from_kernel_flags(it->events);
        }
        void watch(FileDescriptor fd, WatchFlags watch_type = WatchRead) {

        }
        void unwatch(FileDescriptor fd, WatchFlags unwatch_type = WatchAll) {
            sort_as_needed();

            pollfd desc;
            desc.fd = fd;
            const auto it = std::lower_bound(fds.begin(), fds.end(), desc, pollfd_compare);
            if (it == fds.end() || it->fd != fd)
                return;

            const short new_kernel_flags = it->events & ~kernel_flags_from_watch_flags(unwatch_type);
            if (new_kernel_flags)
                it->events = new_kernel_flags;
            else
                fds.erase(it);
        }
        int close(FileDescriptor fd) {
            unwatch(fd);
            return ::close(fd);
        }
        void close_all() {

        }

        template<typename Fn>
        int poll(Fn fn) const {
            int ready = ::poll(fds.data(), fds.size(), -1);
            if (ready < 0)
                return errno;

            for (FileDescriptor fd = 0; ready && fd < max_descriptor; ++fd) {
                WatchFlags watch = 0;

                watch |= FD_ISSET(fd, &read_set)? WatchRead: 0;
                watch |= FD_ISSET(fd, &write_set)? WatchWrite: 0;
                watch |= FD_ISSET(fd, &except_set)? WatchExcept: 0;

                if (watch) {
                    --ready;

                    fn(fd, watch);
                }
            }

            return 0;
        }
    };
}
#endif

#endif // POLL_H
