#ifndef POLL_H
#define POLL_H

#include <chrono>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cerrno>

#include "../system/includes.h"
#include "common.h"

#if POSIX_OS
#include <sys/poll.h>

namespace Skate {
    class Poll : public SocketWatcher {
        std::vector<pollfd> fds;

    private:
        static WatchFlags watch_flags_from_kernel_flags(short kernel_flags) noexcept {
            WatchFlags watch_flags = 0;

            watch_flags |= kernel_flags & POLLIN? WatchRead: 0;
            watch_flags |= kernel_flags & POLLOUT? WatchWrite: 0;
            watch_flags |= kernel_flags & POLLPRI? WatchExcept: 0;
            watch_flags |= kernel_flags & POLLERR? WatchError: 0;
            watch_flags |= kernel_flags & POLLHUP? WatchHangup: 0;
            watch_flags |= kernel_flags & POLLNVAL? WatchInvalid: 0;

            return watch_flags;
        }

        static short kernel_flags_from_watch_flags(WatchFlags watch_flags) noexcept {
            short kernel_flags = 0;

            kernel_flags |= watch_flags & WatchRead? POLLIN: 0;
            kernel_flags |= watch_flags & WatchWrite? POLLOUT: 0;
            kernel_flags |= watch_flags & WatchExcept? POLLPRI: 0;

            return kernel_flags;
        }

    public:
        Poll() {}

        // Returns which watch types are being watched for the given file descriptor,
        // or 0 if the descriptor is not being watched.
        WatchFlags watching(SocketDescriptor fd) const {
            const auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it == fds.end())
                return 0;

            return watch_flags_from_kernel_flags(it->events);
        }

        // Adds a file descriptor to the set to be watched with the specified watch types
        // The descriptor must not already exist in the set, or the behavior is undefined
        void watch(SocketDescriptor fd, WatchFlags watch_type) {
            try_watch(fd, watch_type); // Never fails unless an exception is thrown (out of memory)
        }
        bool try_watch(SocketDescriptor fd, WatchFlags watch_type = WatchRead) {
            pollfd desc;

            desc.fd = fd;
            desc.events = kernel_flags_from_watch_flags(watch_type);

            fds.push_back(desc);
            return true;
        }

        // Removes a file descriptor from all watch types
        // If the descriptor is not in the set, nothing happens
        void unwatch(SocketDescriptor fd) {
            const auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it == fds.end())
                return;

            fds.erase(it);
        }

        // Clears the set and removes all watched descriptors
        void clear() {
            fds.clear();
        }

        // Runs poll() on this set, with a callback function `void (SocketDescriptor, WatchFlags)`
        // The callback function is called with each file descriptor that has a change, as well as what status the descriptor is in (in WatchFlags)
        // Returns 0 on success, or a system error if an error occurs
        int poll(SocketWatcher::NativeWatchFunction fn) {
            const int ready = ::poll(fds.data(), fds.size(), -1);
            if (ready < 0)
                return errno;

            for (const pollfd &desc: fds) {
                WatchFlags watch_flags = watch_flags_from_kernel_flags(desc.revents);

                if (watch_flags)
                    fn(desc.fd, watch_flags);
            }

            return ready? 0: ErrorTimedOut;
        }

        int poll(SocketWatcher::NativeWatchFunction fn, std::chrono::microseconds timeout) {
            if (timeout.count() > INT_MAX)
                return EINVAL;

            const int ready = ::poll(fds.data(), fds.size(), timeout.count() * 1000);
            if (ready < 0)
                return errno;

            for (const pollfd &desc: fds) {
                const WatchFlags watch_flags = watch_flags_from_kernel_flags(desc.revents);

                if (watch_flags)
                    fn(desc.fd, watch_flags);
            }

            return ready? 0: ErrorTimedOut;
        }
    };
}
#elif WINDOWS_OS // End of POSIX_OS
namespace Skate {
    class Poll : public SocketWatcher {
        std::vector<struct pollfd> fds;

    private:
        static WatchFlags watch_flags_from_kernel_flags(short kernel_flags) noexcept {
            WatchFlags watch_flags = 0;

            watch_flags |= kernel_flags & POLLIN? WatchRead: 0;
            watch_flags |= kernel_flags & POLLOUT? WatchWrite: 0;
            watch_flags |= kernel_flags & POLLPRI? WatchExcept: 0;
            watch_flags |= kernel_flags & POLLERR? WatchError: 0;
            watch_flags |= kernel_flags & POLLHUP? WatchHangup: 0;
            watch_flags |= kernel_flags & POLLNVAL? WatchInvalid: 0;

            return watch_flags;
        }

        static short kernel_flags_from_watch_flags(WatchFlags watch_flags) noexcept {
            short kernel_flags = 0;

            kernel_flags |= watch_flags & WatchRead? POLLIN: 0;
            kernel_flags |= watch_flags & WatchWrite? POLLOUT: 0;
            kernel_flags |= watch_flags & WatchExcept? POLLPRI: 0;

            return kernel_flags;
        }

    public:
        Poll() {}

        // Returns which watch types are being watched for the given file descriptor,
        // or 0 if the descriptor is not being watched.
        WatchFlags watching(SocketDescriptor fd) const {
            const auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it == fds.end())
                return 0;

            return watch_flags_from_kernel_flags(it->events);
        }

        // Adds a file descriptor to the set to be watched with the specified watch types
        // The descriptor must not already exist in the set, or the behavior is undefined
        void watch(SocketDescriptor fd, WatchFlags watch_type = WatchRead) {
            try_watch(fd, watch_type); // Never fails unless an exception is thrown (out of memory)
        }
        bool try_watch(SocketDescriptor fd, WatchFlags watch_type = WatchRead) {
            pollfd desc;

            desc.fd = fd;
            desc.events = kernel_flags_from_watch_flags(watch_type);

            fds.push_back(desc);
            return true;
        }

        // Removes a file descriptor from all watch types
        // If the descriptor is not in the set, nothing happens
        void unwatch(SocketDescriptor fd) {
            auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it == fds.end())
                return;

            const size_t last_element = fds.size() - 1;
            std::swap(fds[last_element], *it); // Swap with last element since the order is irrelevant
            fds.resize(last_element);
        }

        // Clears the set and removes all watched descriptors
        void clear() {
            fds.clear();
        }

        // Runs poll() on this set, with a callback function `void (SocketDescriptor, WatchFlags)`
        // The callback function is called with each file descriptor that has a change, as well as what status the descriptor is in (in WatchFlags)
        // Returns 0 on success, or a system error if an error occurs
        int poll(SocketWatcher::NativeWatchFunction fn) {
            const int ready = WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), -1);
            if (ready < 0)
                return WSAGetLastError();

            for (const pollfd &desc: fds) {
                WatchFlags watch_flags = watch_flags_from_kernel_flags(desc.revents);

                if (watch_flags)
                    fn(desc.fd, watch_flags);
            }

            return ready? 0: ErrorTimedOut;
        }

        int poll(SocketWatcher::NativeWatchFunction fn, std::chrono::milliseconds timeout) {
            if (timeout.count() > INT_MAX)
                return EINVAL;

            const int ready = WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), timeout.count());
            if (ready < 0)
                return WSAGetLastError();

            for (const pollfd &desc: fds) {
                const WatchFlags watch_flags = watch_flags_from_kernel_flags(desc.revents);

                if (watch_flags)
                    fn(desc.fd, watch_flags);
            }

            return ready? 0: ErrorTimedOut;
        }
    };
}
#endif

// Generic
namespace Skate {

}

#endif // POLL_H
