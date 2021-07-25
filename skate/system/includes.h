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
    // Really no difference between FileDescriptor and SocketDescriptor on Unix/POSIX systems
    typedef int FileDescriptor;
    typedef int SocketDescriptor;

    static constexpr int ErrorTimedOut = ETIMEDOUT;

    // Startup wrapper for Skate, requires a single object to be defined either statically, or at the start of main()
    class StartupWrapper {
    public:
        StartupWrapper() {
            ::signal(SIGPIPE, SIG_IGN);
        }
    };

    inline ApiString system_error_string(int system_error) {
        size_t buf_size = 256;
        char *buf = static_cast<char *>(malloc(buf_size));
        if (buf == nullptr)
            return {};

        while (1) {
            int string_error = 0;
            errno = 0;

            // See https://linux.die.net/man/3/strerror_r (XSI-compliant vs GNU implementation)
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
            // XSI-compliant version

            string_error = strerror_r(system_error, buf, buf_size);
            if (string_error == 0) // Success
                return ApiString(buf, ApiString::ApiFree);
            else if (string_error < 0) // Old, sets errno for error that occurred while attempting to get string
                string_error = errno;
#else
            // GNU version

            char *gnu_buf = strerror_r(system_error, buf, buf_size);
            if (gnu_buf != buf) { // Using const system string?
                free(buf);
                return ApiString(gnu_buf, ApiString::ApiNoFree);
            } else if (errno == 0)
                return ApiString(buf, ApiString::ApiFree);
            else
                string_error = errno;
#endif

            if (string_error != ERANGE) {
                free(buf);
                return {};
            }

            buf_size *= 2;
            char *new_buf = static_cast<char *>(realloc(buf, buf_size));
            if (new_buf == nullptr) {
                free(buf);
                return {};
            }

            buf = new_buf;
        }
    }
    inline std::string system_error_string_utf8(int system_error) { return system_error_string(system_error); }

    inline std::string system_error_string() { return system_error_string(errno); }
    inline std::string system_error_string_utf8() { return system_error_string(); }
}
#elif WINDOWS_OS
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0600
# endif

# ifndef NOMINMAX
#  define NOMINMAX
# endif

# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>

// Necessary for Event classes
# include <chrono>
# include <memory>
# include <vector>
# include <string>

# if MSVC_COMPILER
#  pragma comment(lib, "ws2_32")
# endif

// Define missing MSG_NOSIGNAL flag to nothing since Windows doesn't use it
#define MSG_NOSIGNAL 0

namespace skate {
    typedef HANDLE FileDescriptor;
    typedef SOCKET SocketDescriptor;

    static constexpr int ErrorTimedOut = ERROR_TIMEOUT;

    // Startup wrapper for Winsock2, embedded in StartupWrapper, so there should be no need to instantiate directly
    class WSAStartupWrapper {
    public:
        WSAStartupWrapper() {
            WSADATA wsaData;

            int err = WSAStartup(MAKEWORD(2, 0), &wsaData);
            if (err)
                throw std::runtime_error("WSAStartup() failed");

            if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 0) {
                WSACleanup();
                throw std::runtime_error("WSAStartup() didn't find a suitable version of Winsock.dll");
            }
        }
        ~WSAStartupWrapper() {
            WSACleanup();
        }
    };

    // Startup wrapper for Skate, requires a single object to be defined either statically, or at the start of main()/WinMain()
    class StartupWrapper {
    public:
        StartupWrapper() {}

    private:
        WSAStartupWrapper wsaStartup;
    };

    inline std::wstring system_error_string(DWORD system_error) {
        LPWSTR str = nullptr;

        if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, system_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&str), 0, NULL)) {
            // For some reason, every message gets a \r\n appended to the end
            const size_t len = wcslen(str);
            if (len >= 2 && str[len-2] == '\r' && str[len-1] == '\n')
                str[len-2] = 0;

            try {
                std::wstring result = str;
                LocalFree(str); str = nullptr;
                return result;
            } catch (...) {
                if (str)
                    LocalFree(str);
                throw;
            }
        }

        return {};
    }
    inline std::string system_error_string_utf8(DWORD system_error) { return to_utf8(system_error_string(system_error)); }

    inline std::wstring system_error_string() { return system_error_string(GetLastError()); }
    inline std::string system_error_string_utf8() { return system_error_string_utf8(GetLastError()); }

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
                throw std::runtime_error(system_error_string_utf8());
        }

        bool is_null() const {return event.get() == nullptr;}
        void signal() {
            if (event && !::SetEvent(event.get()))
                throw std::runtime_error(system_error_string_utf8());
        }
        void reset() {
            if (event && !::ResetEvent(event.get()))
                throw std::runtime_error(system_error_string_utf8());
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
                throw std::runtime_error(system_error_string_utf8());
        }
        // Waits for event to be signalled. If true, event was signalled. If false, timeout occurred. An exception is thrown if an error occurs.
        bool wait(std::chrono::milliseconds timeout) {
            if (timeout.count() >= INFINITE)
                throw std::runtime_error("wait() called with invalid timeout value");

            const DWORD result = ::WaitForSingleObject(event.get(), static_cast<DWORD>(timeout.count()));

            switch (result) {
                case WAIT_FAILED:
                    throw std::runtime_error(system_error_string_utf8());
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
            return !operator==(other);
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
                    throw std::runtime_error(system_error_string_utf8());
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
                    throw std::runtime_error(system_error_string_utf8());
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
