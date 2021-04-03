#ifndef SKATE_SELECT_H
#define SKATE_SELECT_H

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

        Select()
            : max_read_descriptor(-1)
            , max_write_descriptor(-1)
            , max_except_descriptor(-1) {
            FD_ZERO(&master_read_set);
            FD_ZERO(&master_write_set);
            FD_ZERO(&master_except_set);
        }

        WatchFlags watching(FileDescriptor fd) const {
            WatchFlags watch = 0;

            watch |= FD_ISSET(fd, &master_read_set)? WatchRead: 0;
            watch |= FD_ISSET(fd, &master_write_set)? WatchWrite: 0;
            watch |= FD_ISSET(fd, &master_except_set)? WatchExcept: 0;

            return watch;
        }
        void watch(FileDescriptor fd, WatchFlags watch_type = WatchRead) {
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
        void unwatch(FileDescriptor fd, WatchFlags unwatch_type = WatchAll) {
            if (unwatch_type & WatchRead) {
                FD_CLR(fd, &master_read_set);
                if (fd == max_read_descriptor)
                    max_read_descriptor = highest_descriptor(&master_read_set, max_read_descriptor);
            }

            if (unwatch_type & WatchWrite) {
                FD_CLR(fd, &master_write_set);
                if (fd == max_write_descriptor)
                    max_write_descriptor = highest_descriptor(&master_write_set, max_write_descriptor);
            }

            if (unwatch_type & WatchExcept) {
                FD_CLR(fd, &master_except_set);
                if (fd == max_except_descriptor)
                    max_except_descriptor = highest_descriptor(&master_except_set, max_except_descriptor);
            }
        }
        int close(FileDescriptor fd) {
            unwatch(fd);
            return ::close(fd);
        }
        void close_all() {
            const FileDescriptor max_descriptor = std::max(max_read_descriptor, std::max(max_write_descriptor, max_except_descriptor));

            for (FileDescriptor fd = 0; fd < max_descriptor + 1; ++fd) {
                if (watching(fd))
                    close(fd);
            }
        }

        template<typename Fn>
        int select(Fn fn) const {
            const FileDescriptor max_descriptor = std::max(max_read_descriptor, std::max(max_write_descriptor, max_except_descriptor));
            fd_set read_set, write_set, except_set;

            memcpy(&read_set, &master_read_set, sizeof(master_read_set));
            memcpy(&write_set, &master_write_set, sizeof(master_write_set));
            memcpy(&except_set, &master_except_set, sizeof(master_except_set));

            int ready = ::select(max_descriptor+1, &read_set, &write_set, &except_set, NULL);
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

#endif // SKATE_SELECT_H
