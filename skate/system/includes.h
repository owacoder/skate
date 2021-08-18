/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_SYSTEM_INCLUDES_H
#define SKATE_SYSTEM_INCLUDES_H

#include "environment.h"
#include "utf.h"

namespace skate {
    using std::begin;
    using std::end;

    template<typename T> struct type_exists : public std::true_type { typedef int type; };
}

#include <stdexcept>
#include <system_error>

#if POSIX_OS
# include <unistd.h>
# include <cstring>
# include <csignal>

namespace skate {
    // Startup wrapper for Skate, requires a single object to be defined either statically, or at the start of main()
    class startup_wrapper {
    public:
        startup_wrapper() {
            ::signal(SIGPIPE, SIG_IGN);
        }
    };
}
#elif WINDOWS_OS
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0600
# endif

# ifndef NOMINMAX
#  define NOMINMAX
# endif

# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif

# include <winsock2.h>
# include <windows.h>

# if MSVC_COMPILER
#  pragma comment(lib, "ws2_32")
# endif

// Necessary for Event classes
# include <chrono>
# include <memory>
# include <vector>
# include <string>

// Define missing MSG_NOSIGNAL flag to nothing since Windows doesn't use it
#define MSG_NOSIGNAL 0

namespace skate {
    static constexpr int ErrorTimedOut = ERROR_TIMEOUT;

    // Startup wrapper for Winsock2, embedded in StartupWrapper, so there should be no need to instantiate directly
    class wsa_startup_wrapper {
    public:
        wsa_startup_wrapper() {
            WSADATA wsaData;

            int err = ::WSAStartup(MAKEWORD(2, 0), &wsaData);
            if (err)
                throw std::runtime_error("WSAStartup() failed");

            if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 0) {
                ::WSACleanup();
                throw std::runtime_error("WSAStartup() didn't find a suitable version of Winsock.dll");
            }
        }
        ~wsa_startup_wrapper() {
            ::WSACleanup();
        }
    };

    // Startup wrapper for Skate, requires a single object to be defined either statically, or at the start of main()/WinMain()
    class startup_wrapper {
    public:
        startup_wrapper() {}

    private:
        wsa_startup_wrapper wsaStartup;
    };

    class win32_error_category_type : public std::error_category {
        virtual const char *name() const noexcept override { return "win32"; }
        virtual std::string message(int ev) const override {
            LPWSTR str = nullptr;

            try {
                if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   NULL,
                                   ev,
                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   reinterpret_cast<LPWSTR>(&str),
                                   0,
                                   NULL)) {
                    // For some reason, every message gets a \r\n appended to the end
                    const size_t len = wcslen(str);
                    if (len >= 2 && str[len-2] == '\r' && str[len-1] == '\n')
                        str[len-2] = 0;

                    std::string result = to_utf8(static_cast<LPCWSTR>(str));
                    ::LocalFree(str);
                    return result;
                }
            } catch (...) {
                ::LocalFree(str);
                throw;
            }

            return {};
        }

#if 0
        // TODO: not an all-inclusive list, should be worked on
        virtual std::error_condition default_error_condition(int i) const noexcept override {
            switch (i) {
                case WSAEAFNOSUPPORT:            return std::errc::address_family_not_supported;
                case WSAEADDRINUSE:              return std::errc::address_in_use;
                case WSAEADDRNOTAVAIL:           return std::errc::address_not_available;
                case WSAEALREADY:                return std::errc::already_connected;
                case : return std::errc::argument_list_too_long;
                case : return std::errc::argument_out_of_domain;
                case : return std::errc::bad_address;
                case WSAEBADF:                   return std::errc::bad_file_descriptor;
                case ERROR_INVALID_MESSAGE:      return std::errc::bad_message;
                case ERROR_BROKEN_PIPE:          return std::errc::broken_pipe;
                case WSAECONNABORTED:            return std::errc::connection_aborted;
                case WSAEISCONN:
                case ERROR_CONNECTION_ACTIVE:    return std::errc::connection_already_in_progress;
                case WSAECONNREFUSED:
                case ERROR_CONNECTION_REFUSED:   return std::errc::connection_refused;
                case WSAECONNRESET:              return std::errc::connection_reset;
                case : return std::errc::cross_device_link;
                case WSAEDESTADDRREQ:            return std::errc::destination_address_required;
                case ERROR_BUSY:                 return std::errc::device_or_resource_busy;
                case ERROR_DIR_NOT_EMPTY:        return std::errc::directory_not_empty;
                case ERROR_BAD_EXE_FORMAT:       return std::errc::executable_format_error;
                case ERROR_FILENAME_EXCED_RANGE: return std::errc::filename_too_long;
                case ERROR_FILE_EXISTS:          return std::errc::file_exists;
                case ERROR_FILE_TOO_LARGE:       return std::errc::file_too_large;
                case ERROR_INVALID_FUNCTION:     return std::errc::function_not_supported;
                case WSAEHOSTDOWN:
                case WSAEHOSTUNREACH:            return std::errc::host_unreachable;
                case : return std::errc::identifier_removed;
                case : return std::errc::illegal_byte_sequence;
                case : return std::errc::inappropriate_io_control_operation;
                case : return std::errc::interrupted;
                case ERROR_BAD_ARGUMENTS:        return std::errc::invalid_argument;
                case ERROR_SEEK:                 return std::errc::invalid_seek;
                case ERROR_READ_FAULT:
                case ERROR_WRITE_FAULT:          return std::errc::io_error;
                case : return std::errc::is_a_directory;
                case : return std::errc::message_size;
                case WSAENETDOWN:                return std::errc::network_down;
                case WSAENETRESET:               return std::errc::network_reset;
                case WSAENETUNREACH:
                case ERROR_NETWORK_UNREACHABLE:  return std::errc::network_unreachable;
                case : return std::errc::not_a_directory;
                case : return std::errc::not_a_socket;
                case : return std::errc::not_a_stream;
                case WSAENOTCONN:                return std::errc::not_connected;
                case ERROR_OUTOFMEMORY:          return std::errc::not_enough_memory;
                case ERROR_NOT_SUPPORTED:        return std::errc::not_supported;
                case WSAENOBUFS:
                case ERROR_BUFFER_OVERFLOW:      return std::errc::no_buffer_space;
                case : return std::errc::no_child_process;
                case : return std::errc::no_link;
                case : return std::errc::no_lock_available;
                case : return std::errc::no_message_available;
                case : return std::errc::no_protocol_option;
                case : return std::errc::no_space_on_device;
                case : return std::errc::no_stream_resources;
                case : return std::errc::no_such_device;
                case : return std::errc::no_such_device_or_address;
                case : return std::errc::no_such_file_or_directory;
                case : return std::errc::no_such_process;
                case ERROR_OPERATION_ABORTED:    return std::errc::operation_canceled;
                case WSAEINPROGRESS:             return std::errc::operation_in_progress;
                case WSAEOPNOTSUPP:              return std::errc::operation_not_permitted;
                case : return std::errc::operation_not_supported;
                case WSAEWOULDBLOCK:             return std::errc::operation_would_block;
                case : return std::errc::owner_dead;
                case ERROR_ACCESS_DENIED:        return std::errc::permission_denied;
                case : return std::errc::protocol_error;
                case : return std::errc::protocol_not_supported;
                case : return std::errc::read_only_file_system;
                case : return std::errc::resource_deadlock_would_occur;
                case : return std::errc::resource_unavailable_try_again;
                case : return std::errc::result_out_of_range;
                case : return std::errc::state_not_recoverable;
                case : return std::errc::stream_timeout;
                case : return std::errc::text_file_busy;
                case WSAETIMEDOUT:
                case ERROR_TIMEOUT:              return std::errc::timed_out;
                case : return std::errc::too_many_files_open;
                case : return std::errc::too_many_files_open_in_system;
                case : return std::errc::too_many_links;
                case : return std::errc::too_many_symbolic_link_levels;
                case : return std::errc::value_too_large;
                case WSAE: return std::errc::wrong_protocol_type;
                default: return std::errc();
            }
        }
#endif
    };

    inline const std::error_category &win32_category() {
        static win32_error_category_type win_category;
        return win_category;
    }

    inline std::error_code win32_error() noexcept {
        const int v = ::GetLastError();
        return std::error_code(v, win32_category());
    }

    class EventList;

    class Event {
        friend class EventList;

        std::shared_ptr<void> event;

    public:
        Event(std::nullptr_t) {}
        Event(bool manual_reset = false, bool signalled = false)
            : event(std::shared_ptr<void>(::CreateEvent(NULL, manual_reset, signalled, NULL),
                                          [](HANDLE event) {
                                              if (event)
                                                  ::CloseHandle(event);
                                          }))
        {
            if (!event)
                throw std::system_error(::GetLastError(), win32_category());
        }

        bool is_null() const {return event.get() == nullptr;}
        void signal() {
            if (event && !::SetEvent(event.get()))
                throw std::system_error(::GetLastError(), win32_category());
        }
        void reset() {
            if (event && !::ResetEvent(event.get()))
                throw std::system_error(::GetLastError(), win32_category());
        }
        void set_state(bool signalled) {
            if (signalled)
                signal();
            else
                reset();
        }

        // Waits for event to be signalled. An exception is thrown if an error occurs.
        void wait() {
            if (::WaitForSingleObject(event.get(), INFINITE) == WAIT_FAILED)
                throw std::system_error(::GetLastError(), win32_category());
        }
        // Waits for event to be signalled. If true, event was signalled. If false, timeout occurred. An exception is thrown if an error occurs.
        bool wait(std::chrono::milliseconds timeout) {
            if (timeout.count() >= INFINITE)
                throw std::runtime_error("wait() called with invalid timeout value");

            const DWORD result = ::WaitForSingleObject(event.get(), static_cast<DWORD>(timeout.count()));

            switch (result) {
                case WAIT_FAILED:
                    throw std::system_error(::GetLastError(), win32_category());
                case WAIT_OBJECT_0:
                    return true;
                default:
                    return false;
            }
        }

        bool operator==(const Event &other) const {
            return event == other.event;
        }
        bool operator!=(const Event &other) const {
            return !(*this == other);
        }
        operator bool() const {
            return !is_null();
        }
    };

    class EventList {
        std::vector<Event> events;
        mutable std::vector<HANDLE> cache;

        void rebuild_cache() const {
            if (cache.empty()) {
                for (const Event &ev: events) {
                    cache.push_back(ev.event.get());
                }
            }
        }

    public:
        EventList() {}

        size_t size() const noexcept {return events.size();}
        EventList &add(Event ev) {
            if (std::find(events.begin(), events.end(), ev) == events.end()) {
                if (events.size() == MAXIMUM_WAIT_OBJECTS)
                    throw std::runtime_error("Attempted to wait on more than " + std::to_string(MAXIMUM_WAIT_OBJECTS) + " events with EventList");

                events.push_back(ev);
                cache.clear();
            }

            return *this;
        }
        EventList &operator<<(const Event &ev) {return add(ev);}
        EventList &remove(Event ev) {
            const auto it = std::find(events.begin(), events.end(), ev);
            if (it != events.end()) {
                events.erase(it);
                cache.clear();
            }

            return *this;
        }

        Event wait() const {
            rebuild_cache();

            const DWORD result = ::WaitForMultipleObjects(static_cast<DWORD>(cache.size()),
                                                          cache.data(),
                                                          FALSE,
                                                          INFINITE);

            switch (result) {
                case WAIT_FAILED:
                    throw std::system_error(::GetLastError(), win32_category());
                default:
                    if (result - WAIT_OBJECT_0 < MAXIMUM_WAIT_OBJECTS)
                        return events[result - WAIT_OBJECT_0];
                    else
                        return events[result - WAIT_ABANDONED_0];
            }
        }
        Event wait(std::chrono::milliseconds timeout) const {
            rebuild_cache();

            if (timeout.count() >= INFINITE)
                throw std::runtime_error("wait() called with invalid timeout value");

            const DWORD result = ::WaitForMultipleObjects(static_cast<DWORD>(cache.size()),
                                                          cache.data(),
                                                          FALSE,
                                                          static_cast<DWORD>(timeout.count()));

            switch (result) {
                case WAIT_FAILED:
                    throw std::system_error(::GetLastError(), win32_category());
                case WAIT_TIMEOUT:
                    return {nullptr};
                default:
                    if (result - WAIT_OBJECT_0 < MAXIMUM_WAIT_OBJECTS)
                        return events[result - WAIT_OBJECT_0];
                    else
                        return events[result - WAIT_ABANDONED_0];
            }
        }
    };
}

// Handy reference for Windows Async programming:
// https://www.microsoftpressstore.com/articles/article.aspx?p=2224047&seqNum=5
// and http://www.kegel.com/c10k.html
#else
# error Platform not supported
#endif

#endif // SKATE_SYSTEM_INCLUDES_H
