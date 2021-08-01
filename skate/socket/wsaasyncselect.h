#ifndef SKATE_WSAASYNCSELECT_H
#define SKATE_WSAASYNCSELECT_H

#include "../system/includes.h"
#include "common.h"

#if WINDOWS_OS
namespace skate {
    class WSAAsyncSelectWatcher : public socket_watcher {
        const HWND hwnd;
        const UINT msg;

    public:
        static socket_watch_flags watch_flags_from_kernel_flags(long kernel_flags) noexcept {
            socket_watch_flags watch_flags = 0;

            watch_flags |= kernel_flags & (FD_READ | FD_ACCEPT)? WatchRead: 0;
            watch_flags |= kernel_flags & FD_WRITE? WatchWrite: 0;
            watch_flags |= kernel_flags & FD_OOB? WatchExcept: 0;
            watch_flags |= kernel_flags & FD_CLOSE? WatchHangup: 0;

            return watch_flags;
        }

        static long kernel_flags_from_watch_flags(socket_watch_flags watch_flags) noexcept {
            long kernel_flags = 0;

            kernel_flags |= watch_flags & WatchRead? FD_READ | FD_ACCEPT: 0;
            kernel_flags |= watch_flags & WatchWrite? FD_WRITE: 0;
            kernel_flags |= watch_flags & WatchExcept? FD_OOB: 0;
            kernel_flags |= watch_flags & WatchHangup? FD_CLOSE: 0;

            return kernel_flags;
        }

        WSAAsyncSelectWatcher(HWND hwnd, UINT msg) : hwnd(hwnd) , msg(msg) {}
        virtual ~WSAAsyncSelectWatcher() {}

        virtual socket_watch_flags watching(system_socket_descriptor) const override {
            // Not able to determine if socket is being listened to
            return 0;
        }

        virtual void watch(std::error_code &ec, system_socket_descriptor socket, socket_watch_flags watch_type) override {
            if (::WSAAsyncSelect(socket, hwnd, msg, kernel_flags_from_watch_flags(watch_type)) == 0)
                ec.clear();
            else
                ec = impl::socket_error();
        }

        virtual void modify(std::error_code &ec, system_socket_descriptor socket, socket_watch_flags new_watch_type) override {
            watch(ec, socket, new_watch_type);
        }

        virtual void unwatch(std::error_code &ec, system_socket_descriptor socket) override {
            if (::WSAAsyncSelect(socket, hwnd, 0, 0) != 0 && WSAGetLastError() != WSAEINVAL && WSAGetLastError() != WSAENOTSOCK) // Clearing the events desired will cancel watching
                ec = impl::socket_error();
            else
                ec.clear();
        }
        virtual void unwatch_dead_descriptor(std::error_code &ec, system_socket_descriptor) override {
            ec.clear();
            /* Do nothing as kernel removes descriptor from WSAAsyncSelect() set when closesocket() is called */
        }

        virtual void clear(std::error_code &ec) override {
            ec.clear();
            /* Do nothing as the kernel doesn't allow clearing all descriptors from the watch */
        }

        void poll(std::error_code &, native_watch_function) {
            throw std::logic_error("poll() should not be called on a WSAAsyncSelectWatcher");
        }
        void poll(std::error_code &, native_watch_function, std::chrono::microseconds) {
            throw std::logic_error("poll() should not be called on a WSAAsyncSelectWatcher");
        }
    };
}
#endif

#endif // SKATE_WSAASYNCSELECT_H
