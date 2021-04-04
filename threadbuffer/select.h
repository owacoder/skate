#ifndef SKATE_SELECT_H
#define SKATE_SELECT_H

#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cerrno>

#include "environment.h"

#if POSIX_OS
# include <sys/select.h>
# include <unistd.h>

namespace Skate {
    class Select
    {
        FileDescriptor max_read_descriptor;
        FileDescriptor max_write_descriptor;
        FileDescriptor max_except_descriptor;

        fd_set master_read_set;
        fd_set master_write_set;
        fd_set master_except_set;

        // Find last/highest descriptor assigned to set, starting from a known high descriptor (or the size of the set if not known)
        // Returns the highest descriptor in the set or -1 if the set is empty
        inline FileDescriptor highest_descriptor(fd_set *fds, FileDescriptor start = FD_SETSIZE) {
            for (start = std::min(start, FD_SETSIZE); start >= 0; --start) {
                if (FD_ISSET(start, fds))
                    return start;
            }

            return -1;
        }

    public:
        using WatchFlags = uint8_t;

        static constexpr WatchFlags WatchRead   = 1 << 0;
        static constexpr WatchFlags WatchWrite  = 1 << 1;
        static constexpr WatchFlags WatchExcept = 1 << 2;
        static constexpr WatchFlags WatchAll = WatchRead | WatchWrite | WatchExcept;

        Select() {clear();}

        // Returns which watch types are being watched for the given file descriptor,
        // or 0 if the file descriptor is not being watched.
        WatchFlags watching(FileDescriptor fd) const {
            if (fd >= FD_SETSIZE)
                return 0;

            WatchFlags watch = 0;

            watch |= FD_ISSET(fd, &master_read_set)? WatchRead: 0;
            watch |= FD_ISSET(fd, &master_write_set)? WatchWrite: 0;
            watch |= FD_ISSET(fd, &master_except_set)? WatchExcept: 0;

            return watch;
        }

        // Adds a file descriptor to the set to be watched with the specified watch types
        // The descriptor must not already exist in the set, or the behavior is undefined
        void watch(FileDescriptor fd, WatchFlags watch_type = WatchRead) {
            if (fd >= FD_SETSIZE)
                throw std::runtime_error("watch() called with file descriptor greater-than or equal to FD_SETSIZE");

            if (watch_type & WatchRead) {
                FD_SET(fd, &master_read_set);
                max_read_descriptor = std::max(fd, max_read_descriptor);
            }

            if (watch_type & WatchWrite) {
                FD_SET(fd, &master_write_set);
                max_write_descriptor = std::max(fd, max_write_descriptor);
            }

            if (watch_type & WatchExcept) {
                FD_SET(fd, &master_except_set);
                max_except_descriptor = std::max(fd, max_except_descriptor);
            }
        }

        // Removes a file descriptor from all watch types
        // If the descriptor is not in the set, nothing happens
        void unwatch(FileDescriptor fd) {
            if (fd >= FD_SETSIZE)
                return;

            FD_CLR(fd, &master_read_set);
            if (fd == max_read_descriptor)
                max_read_descriptor = highest_descriptor(&master_read_set, max_read_descriptor);

            FD_CLR(fd, &master_write_set);
            if (fd == max_write_descriptor)
                max_write_descriptor = highest_descriptor(&master_write_set, max_write_descriptor);

            FD_CLR(fd, &master_except_set);
            if (fd == max_except_descriptor)
                max_except_descriptor = highest_descriptor(&master_except_set, max_except_descriptor);
        }

        // Clears the set and removes all watched descriptors
        void clear() {
            FD_ZERO(&master_read_set);
            FD_ZERO(&master_write_set);
            FD_ZERO(&master_except_set);

            max_read_descriptor = -1;
            max_write_descriptor = -1;
            max_except_descriptor = -1;
        }

        // Closes the specified file descriptor, unwatching it from all watch types
        // Returns 0 on success, or a system error if an error occurs
        int close(FileDescriptor fd) {
            unwatch(fd);
            return ::close(fd);
        }

        // Closes all file descriptors in the set, unwatching them from all watch types
        void close_all() {
            const FileDescriptor max_descriptor = std::max(max_read_descriptor, std::max(max_write_descriptor, max_except_descriptor));

            for (FileDescriptor fd = 0; fd < max_descriptor + 1; ++fd) {
                if (watching(fd))
                    close(fd);
            }

            clear();
        }

        // Runs select() on this set, with a callback function `void (FileDescriptor, WatchFlags)`
        // The callback function is called with each file descriptor that has a change, as well as what status the descriptor is in (in WatchFlags)
        // Returns 0 on success, or a system error if an error occurs
        template<typename Fn>
        int select(Fn fn, struct timeval *tm) const {
            const FileDescriptor max_descriptor = std::max(max_read_descriptor, std::max(max_write_descriptor, max_except_descriptor));
            fd_set read_set, write_set, except_set;

            memcpy(&read_set, &master_read_set, sizeof(master_read_set));
            memcpy(&write_set, &master_write_set, sizeof(master_write_set));
            memcpy(&except_set, &master_except_set, sizeof(master_except_set));

            int ready = ::select(max_descriptor+1, &read_set, &write_set, &except_set, tm);
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

        template<typename Fn>
        int select(Fn fn) const {
            return select(fn, NULL);
        }

        template<typename Fn>
        int select(Fn fn, std::chrono::microseconds us) const {
            struct timeval tm;

            tm.tv_sec = us.count() / 1000000;
            tm.tv_usec = us.count() % 1000000;

            return select(fn, &tm);
        }
    };
}
#endif

#endif // SKATE_SELECT_H
