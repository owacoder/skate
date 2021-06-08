#ifndef SKATE_SELECT_H
#define SKATE_SELECT_H

#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cerrno>

#include <memory>
#include <unordered_map>

#include "../system/includes.h"
#include "common.h"

#if POSIX_OS
# include <sys/select.h>

namespace Skate {
    class Select : public SocketWatcher {
        SocketDescriptor max_read_descriptor;
        SocketDescriptor max_write_descriptor;
        SocketDescriptor max_except_descriptor;

        fd_set master_read_set;
        fd_set master_write_set;
        fd_set master_except_set;

        // Find last/highest descriptor assigned to set, starting from a known high descriptor (or the size of the set if not known)
        // Returns the highest descriptor in the set or -1 if the set is empty
        inline SocketDescriptor highest_descriptor(fd_set *fds, SocketDescriptor start = FD_SETSIZE) {
            for (start = std::min(start, FD_SETSIZE); start >= 0; --start) {
                if (FD_ISSET(start, fds))
                    return start;
            }

            return -1;
        }

    public:
        Select()
            : max_read_descriptor(-1)
            , max_write_descriptor(-1)
            , max_except_descriptor(-1)
        {
            FD_ZERO(&master_read_set);
            FD_ZERO(&master_write_set);
            FD_ZERO(&master_except_set);
        }
        virtual ~Select() {}

        // Returns which watch types are being watched for the given file descriptor,
        // or 0 if the file descriptor is not being watched.
        WatchFlags watching(SocketDescriptor fd) const {
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
        void watch(SocketDescriptor fd, WatchFlags watch_type = WatchRead) {
            if (!try_watch(fd, watch_type))
                throw std::runtime_error("watch() called with file descriptor greater-than or equal to FD_SETSIZE"); // The only failure reason
        }
        bool try_watch(SocketDescriptor fd, WatchFlags watch_type = WatchRead) {
            if (fd >= FD_SETSIZE)
                return false;

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

            return true;
        }

        // Removes a file descriptor from all watch types
        // If the descriptor is not in the set, nothing happens
        void unwatch(SocketDescriptor fd) {
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

        // Runs select() on this set, with a callback function `void (SocketDescriptor, WatchFlags)`
        // The callback function is called with each file descriptor that has a change, as well as what status the descriptor is in (in WatchFlags)
        // Returns 0 on success, Skate::ErrorTimedOut on timeout, or a system error if an error occurs
        int poll(SocketWatcher::NativeWatchFunction fn, struct timeval *timeout) {
            const SocketDescriptor max_descriptor = std::max(max_read_descriptor, std::max(max_write_descriptor, max_except_descriptor));
            fd_set read_set, write_set, except_set;

            memcpy(&read_set, &master_read_set, sizeof(master_read_set));
            memcpy(&write_set, &master_write_set, sizeof(master_write_set));
            memcpy(&except_set, &master_except_set, sizeof(master_except_set));

            const int result = ::select(max_descriptor+1, &read_set, &write_set, &except_set, timeout);
            if (result < 0)
                return errno;

            int ready = result;
            for (SocketDescriptor fd = 0; ready && fd <= max_descriptor; ++fd) {
                WatchFlags watch = 0;

                watch |= FD_ISSET(fd, &read_set)? WatchRead: 0;
                watch |= FD_ISSET(fd, &write_set)? WatchWrite: 0;
                watch |= FD_ISSET(fd, &except_set)? WatchExcept: 0;

                if (watch) {
                    --ready;

                    fn(fd, watch);
                }
            }

            return result? 0: ErrorTimedOut;
        }

        int poll(NativeWatchFunction fn) {
            return poll(fn, nullptr);
        }
        int poll(SocketWatcher::NativeWatchFunction fn, std::chrono::microseconds timeout) {
            struct timeval tm;

            tm.tv_sec = timeout.count() / 1000000;
            tm.tv_usec = timeout.count() % 1000000;

            return poll(fn, &tm);
        }
    };
}
#elif WINDOWS_OS // End of POSIX_OS
namespace Skate {
    class Select : public SocketWatcher {
        fd_set master_read_set;
        fd_set master_write_set;
        fd_set master_except_set;

        // Runs fn() on these sets, as a callback function `void (SocketDescriptor, WatchFlags)`
        template<typename Fn>
        static void for_all_descriptors(Fn fn, fd_set &read_set, fd_set &write_set, fd_set &except_set) {
            if (write_set.fd_count == 0 && except_set.fd_count == 0) {
                for (u_int i = 0; i < read_set.fd_count; ++i)
                    fn(read_set.fd_array[i], WatchRead);
                return;
            } else if (read_set.fd_count == 0 && except_set.fd_count == 0) {
                for (u_int i = 0; i < write_set.fd_count; ++i)
                    fn(write_set.fd_array[i], WatchWrite);
                return;
            } else if (read_set.fd_count == 0 && write_set.fd_count == 0) {
                for (u_int i = 0; i < except_set.fd_count; ++i)
                    fn(except_set.fd_array[i], WatchExcept);
                return;
            }

            std::sort(  read_set.fd_array,   read_set.fd_array +   read_set.fd_count);
            std::sort( write_set.fd_array,  write_set.fd_array +  write_set.fd_count);
            std::sort(except_set.fd_array, except_set.fd_array + except_set.fd_count);

            u_int   read_start = 0;
            u_int  write_start = 0;
            u_int except_start = 0;

            while ((  read_start <   read_set.fd_count) ||
                   ( write_start <  write_set.fd_count) ||
                   (except_start < except_set.fd_count)) {
                const SocketDescriptor sock_read   =   read_start <   read_set.fd_count?   read_set.fd_array[  read_start]: INVALID_SOCKET;
                const SocketDescriptor sock_write  =  write_start <  write_set.fd_count?  write_set.fd_array[ write_start]: INVALID_SOCKET;
                const SocketDescriptor sock_except = except_start < except_set.fd_count? except_set.fd_array[except_start]: INVALID_SOCKET;
                const SocketDescriptor sock_lowest = std::min(sock_read, std::min(sock_write, sock_except));

                WatchFlags flags = 0;

                if (sock_read == sock_lowest) {
                    flags |= WatchRead;
                    ++read_start;
                }

                if (sock_write == sock_lowest) {
                    flags |= WatchWrite;
                    ++write_start;
                }

                if (sock_except == sock_lowest) {
                    flags |= WatchExcept;
                    ++except_start;
                }

                fn(sock_lowest, flags);
            }
        }

    public:
        Select() {
            FD_ZERO(&master_read_set);
            FD_ZERO(&master_write_set);
            FD_ZERO(&master_except_set);
        }
        virtual ~Select() {}

        // Returns which watch types are being watched for the given socket descriptor,
        // or 0 if the file descriptor is not being watched.
        WatchFlags watching(SocketDescriptor fd) const {
            WatchFlags watch = 0;

            watch |= FD_ISSET(fd, &master_read_set)? WatchRead: 0;
            watch |= FD_ISSET(fd, &master_write_set)? WatchWrite: 0;
            watch |= FD_ISSET(fd, &master_except_set)? WatchExcept: 0;

            return watch;
        }

        // Adds a file descriptor to the set to be watched with the specified watch types
        // The descriptor must not already exist in the set, or the behavior is undefined
        void watch(SocketDescriptor fd, WatchFlags watch_type = WatchRead) {
            if (!try_watch(fd, watch_type))
                throw std::runtime_error("watch() called with FD_SETSIZE descriptors already being watched"); // The only failure reason
        }
        bool try_watch(SocketDescriptor fd, WatchFlags watch_type = WatchRead) {
            if (  master_read_set.fd_count == FD_SETSIZE && (watch_type & WatchRead  )) return false;
            if ( master_write_set.fd_count == FD_SETSIZE && (watch_type & WatchWrite )) return false;
            if (master_except_set.fd_count == FD_SETSIZE && (watch_type & WatchExcept)) return false;

            if (watch_type & WatchRead)
                FD_SET(fd, &master_read_set);

            if (watch_type & WatchWrite)
                FD_SET(fd, &master_write_set);

            if (watch_type & WatchExcept)
                FD_SET(fd, &master_except_set);

            return true;
        }

        // Removes a file descriptor from all watch types
        // If the descriptor is not in the set, nothing happens
        void unwatch(SocketDescriptor fd) {
            FD_CLR(fd, &master_read_set);
            FD_CLR(fd, &master_write_set);
            FD_CLR(fd, &master_except_set);
        }

        // Clears the set and removes all watched descriptors
        void clear() {
            FD_ZERO(&master_read_set);
            FD_ZERO(&master_write_set);
            FD_ZERO(&master_except_set);
        }

        // Runs select() on this set, with a callback function `void (SocketDescriptor, WatchFlags)`
        // The callback function is called with each file descriptor that has a change, as well as what status the descriptor is in (in WatchFlags)
        // Returns 0 on success, Skate::ErrorTimedOut on timeout, or a system error if an error occurs
        int poll(SocketWatcher::NativeWatchFunction fn, struct timeval *timeout) {
            fd_set read_set, write_set, except_set;

            memcpy(&read_set, &master_read_set, sizeof(master_read_set));
            memcpy(&write_set, &master_write_set, sizeof(master_write_set));
            memcpy(&except_set, &master_except_set, sizeof(master_except_set));

            int ready = ::select(0, &read_set, &write_set, &except_set, timeout);
            if (ready < 0)
                return WSAGetLastError();

            for_all_descriptors(fn, read_set, write_set, except_set);

            return ready? 0: ErrorTimedOut;
        }

        int poll(SocketWatcher::NativeWatchFunction fn) {
            return poll(fn, nullptr);
        }
        int poll(SocketWatcher::NativeWatchFunction fn, std::chrono::microseconds timeout) {
            struct timeval tm;

            tm.tv_sec = static_cast<long>(timeout.count() / 1000000);
            tm.tv_usec = timeout.count() % 1000000;

            return poll(fn, &tm);
        }
    };
}
#endif // WINDOWS_OS

#endif // SKATE_SELECT_H
