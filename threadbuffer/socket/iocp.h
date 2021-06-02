#ifndef SKATE_IOCP_H
#define SKATE_IOCP_H

#include "../system/includes.h"
#include "common.h"

#if WINDOWS_OS
namespace Skate {
    class IOCP : public SocketWatcher {
        HANDLE completion_port;

    public:
        IOCP(DWORD thread_count = 0) : completion_port(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, thread_count)) {
            if (completion_port == NULL)
                throw std::runtime_error(system_error_string(GetLastError()).to_utf8());
        }
        virtual ~IOCP() { ::CloseHandle(completion_port); }

        // Returns which watch types are being watched for the given file descriptor,
        // or 0 if the descriptor is not being watched.
        WatchFlags watching(SocketDescriptor fd) const {
            return 0;
        }

        // Adds a file descriptor to the set to be watched with the specified watch types
        // The descriptor must not already exist in the set, or the behavior is undefined
        void watch(SocketDescriptor fd, WatchFlags watch_type = WatchRead) {
            if (!try_watch(fd, watch_type))
                throw std::runtime_error(system_error_string(GetLastError()).to_utf8());
        }
        bool try_watch(SocketDescriptor fd, WatchFlags watch_type = WatchRead) {
            return completion_port == ::CreateIoCompletionPort(fd, completion_port, 0, 0);
        }

        // Modifies the watch flags for a descriptor in the set
        // If the descriptor is not in the set, nothing happens
        void modify(SocketDescriptor fd, WatchFlags new_watch_type) {
            auto it = std::find_if(fds.begin(), fds.end(), [fd](const pollfd &desc) {return desc.fd == fd;});
            if (it == fds.end())
                return;

            it->events = kernel_flags_from_watch_flags(new_watch_type);
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

            return 0;
        }

        int poll(SocketWatcher::NativeWatchFunction fn, std::chrono::microseconds timeout) {
            if (timeout.count() > INT_MAX)
                return EINVAL;

            const int ready = WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), static_cast<long>(timeout.count() / 1000));
            if (ready < 0)
                return WSAGetLastError();

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

            return ready? 0: ErrorTimedOut;
        }
    };
}
#endif // WINDOWS_OS

#endif // SKATE_IOCP_H
