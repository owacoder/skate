#ifndef SKATE_WSAASYNCSELECT_H
#define SKATE_WSAASYNCSELECT_H

#include "../system/includes.h"
#include "common.h"

#if WINDOWS_OS
namespace skate {
    class WSAAsyncSelectWatcher : public SocketWatcher {
        const HWND hwnd;
        const UINT msg;

    public:
        static WatchFlags watch_flags_from_kernel_flags(long kernel_flags) noexcept {
            WatchFlags watch_flags = 0;

            watch_flags |= kernel_flags & (FD_READ | FD_ACCEPT)? WatchRead: 0;
            watch_flags |= kernel_flags & FD_WRITE? WatchWrite: 0;
            watch_flags |= kernel_flags & FD_OOB? WatchExcept: 0;
            watch_flags |= kernel_flags & FD_CLOSE? WatchHangup: 0;

            return watch_flags;
        }

        static long kernel_flags_from_watch_flags(WatchFlags watch_flags) noexcept {
            long kernel_flags = 0;

            kernel_flags |= watch_flags & WatchRead? FD_READ | FD_ACCEPT: 0;
            kernel_flags |= watch_flags & WatchWrite? FD_WRITE: 0;
            kernel_flags |= watch_flags & WatchExcept? FD_OOB: 0;
            kernel_flags |= watch_flags & WatchHangup? FD_CLOSE: 0;

            return kernel_flags;
        }

        WSAAsyncSelectWatcher(HWND hwnd, UINT msg)
            : hwnd(hwnd)
            , msg(msg)
        {}
        virtual ~WSAAsyncSelectWatcher() {}

        WatchFlags watching(SocketDescriptor) const {
            // Not able to determine if socket is being listened to
            return 0;
        }

        void watch(SocketDescriptor socket, WatchFlags watch_type) {
            if (!try_watch(socket, watch_type))
                throw std::runtime_error(system_error_string(WSAGetLastError()).to_utf8());
        }

        bool try_watch(SocketDescriptor socket, WatchFlags watch_type) {
            return ::WSAAsyncSelect(socket, hwnd, msg, kernel_flags_from_watch_flags(watch_type)) == 0;
        }

        void modify(SocketDescriptor socket, WatchFlags new_watch_type) {
            watch(socket, new_watch_type);
        }

        void unwatch(SocketDescriptor socket) {
            if (::WSAAsyncSelect(socket, hwnd, 0, 0) != 0 && WSAGetLastError() != WSAEINVAL && WSAGetLastError() != WSAENOTSOCK) // Clearing the events desired will cancel watching
                throw std::runtime_error(system_error_string(WSAGetLastError()).to_utf8());
        }
        void unwatch_dead_descriptor(SocketDescriptor) {
            /* Do nothing as kernel removes descriptor from WSAAsyncSelect() set when closesocket() is called */
        }

        void clear() {
            /* Do nothing as the kernel doesn't allow clearing all descriptors from the watch */
        }

        int poll(NativeWatchFunction) {
            throw std::logic_error("poll() should not be called on a WSAAsyncSelectWatcher");
            return 0;
        }
        int poll(NativeWatchFunction, std::chrono::microseconds) {
            throw std::logic_error("poll() should not be called on a WSAAsyncSelectWatcher");
            return 0;
        }
    };
}
#endif

#endif // SKATE_WSAASYNCSELECT_H
