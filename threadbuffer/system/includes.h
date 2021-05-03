#ifndef SKATE_SYSTEM_INCLUDES_H
#define SKATE_SYSTEM_INCLUDES_H

#include "environment.h"

#include <stdexcept>

#if POSIX_OS
# include <unistd.h>
# include <cstring>
# include <csignal>

namespace Skate {
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

    class ApiString {
    public:
        enum DestroyType {
            ApiNoFree,
            ApiDeleteArray,
            ApiFree
        };

        ApiString() : str(const_cast<char *>("")), len(0), type(ApiNoFree) {}
        ApiString(const char *data) : str(nullptr), len(0), type(ApiNoFree) {
            const size_t sz = strlen(data) + 1;
            char *copy = new char[sz];
            memcpy(copy, data, sz * sizeof(*copy));

            str = copy;
            len = sz;
            type = ApiDeleteArray;
        }
        ApiString(char *data, DestroyType type) : str(data), len(strlen(data)), type(type) {}
        ApiString(const std::string &data) : ApiString(data.c_str()) {}
        ApiString(const ApiString &other) : str(nullptr), len(0), type(ApiNoFree) {
            const size_t sz = other.size() + 1;
            char *copy = new char[sz];
            memcpy(copy, other.data(), sz * sizeof(*copy));

            str = copy;
            len = sz;
            type = ApiDeleteArray;
        }
        ApiString(ApiString &&other) : str(other.str), len(other.len), type(other.type) {
            other.str = nullptr;
            other.len = 0;
            other.type = ApiNoFree;
        }
        ~ApiString() {destroy();}

        ApiString &operator=(const ApiString &other) {
            if (&other == this)
                return *this;

            destroy();

            const size_t sz = other.size() + 1;
            char *copy = new char[sz];
            memcpy(copy, other.data(), sz * sizeof(*copy));

            str = copy;
            len = sz;
            type = ApiDeleteArray;

            return *this;
        }
        ApiString &operator=(ApiString &&other) {
            if (&other == this)
                return *this;

            destroy();

            str = other.str; other.str = nullptr;
            len = other.len; other.len = 0;
            type = other.type; other.type = ApiNoFree;

            return *this;
        }

        static ApiString from_utf8(const std::string &utf8) {return ApiString(utf8.c_str());}
        static ApiString from_utf8(const char *utf8) {return ApiString(utf8);}

        std::string to_utf8() const {
            return str;
        }
        operator std::string() const {
            return to_utf8();
        }

        const char *data() const {return str;}
        operator const char *() const {
            return str;
        }

        size_t size() const {return len;}
        size_t length() const {return len;}

        bool operator==(const ApiString &other) const {
            return len == other.len && memcmp(str, other.str, len * sizeof(*str)) == 0;
        }
        bool operator!=(const ApiString &other) const {
            return !operator==(other);
        }

    public:
        char *str;
        size_t len;
        DestroyType type;

        void destroy() {
            switch (type) {
                default: break;
                case ApiDeleteArray: delete[] str; break;
                case ApiFree: free(str); break;
            }
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
}
#elif WINDOWS_OS
# define _WIN32_WINNT 0x0600

# ifndef NOMINMAX
#  define NOMINMAX
# endif

# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>

# if MSVC_COMPILER
#  pragma comment(lib, "ws2_32.lib")
# endif

namespace Skate {
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
        StartupWrapper() {
            ::signal(SIGPIPE, SIG_IGN);
        }

    private:
        WSAStartupWrapper wsaStartup;
    };

    // A simple system NUL-terminated API string class that is just used for converting to and from system strings.
    // Allows various forms of destroying the contained string, including Windows-specific GlobalFree and LocalFree
    // Can be converted to and from std::strings formatted in UTF-8.
    class ApiString {
    public:
        enum DestroyType {
            ApiNoFree,
            ApiLocalFree,
            ApiGlobalFree,
            ApiDeleteArray,
            ApiFree
        };

        ApiString() : str(const_cast<LPWSTR>(L"")), len(0), type(ApiNoFree) {}
        ApiString(LPCWSTR data) : str(nullptr), len(0), type(ApiNoFree) {
            const size_t sz = wcslen(data) + 1;
            LPWSTR copy = new WCHAR[sz];
            memcpy(copy, data, sz * sizeof(*copy));

            str = copy;
            len = sz;
            type = ApiDeleteArray;
        }
        ApiString(LPWSTR data, DestroyType type) : str(data), len(wcslen(data)), type(type) {}
        ApiString(const char *utf8) : str(nullptr), len(0), type(ApiNoFree) {
            const char *error = "Cannot create ApiString from UTF-8";

            const int chars = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
            if (!chars)
                throw std::runtime_error(error);

            LPWSTR result = new WCHAR[chars];

            if (!MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, utf8, -1, result, chars)) {
                delete[] result;
                throw std::runtime_error(error);
            }

            str = result;
            len = chars;
            type = ApiDeleteArray;
        }
        ApiString(const std::string &utf8) : ApiString(utf8.c_str()) {}
        ApiString(const ApiString &other) : str(nullptr), len(0), type(ApiNoFree) {
            const size_t sz = other.size() + 1;
            LPWSTR copy = new WCHAR[sz];
            memcpy(copy, other.data(), sz * sizeof(*copy));

            str = copy;
            len = sz;
            type = ApiDeleteArray;
        }
        ApiString(ApiString &&other) : str(other.str), len(other.len), type(other.type) {
            other.str = nullptr;
            other.len = 0;
            other.type = ApiNoFree;
        }
        ~ApiString() {destroy();}

        ApiString &operator=(const ApiString &other) {
            if (&other == this)
                return *this;

            destroy();

            const size_t sz = other.size() + 1;
            LPWSTR copy = new WCHAR[sz];
            memcpy(copy, other.data(), sz * sizeof(*copy));

            str = copy;
            len = sz;
            type = ApiDeleteArray;

            return *this;
        }
        ApiString &operator=(ApiString &&other) {
            if (&other == this)
                return *this;

            destroy();

            str = other.str; other.str = nullptr;
            len = other.len; other.len = 0;
            type = other.type; other.type = ApiNoFree;

            return *this;
        }

        static ApiString from_utf8(const std::string &utf8) {return ApiString(utf8);}
        static ApiString from_utf8(const char *utf8) {return ApiString(utf8);}

        std::string to_utf8() const {
            const char *error = "Cannot convert ApiString to UTF-8";

            std::string result;
            const size_t len = wcslen(str);
            if (len > INT_MAX)
                throw std::runtime_error(error);

            const int bytes = WideCharToMultiByte(CP_UTF8, 0, str, static_cast<int>(len), NULL, 0, NULL, NULL);
            if (!bytes)
                throw std::runtime_error(error);

            result.resize(bytes);

            if (!WideCharToMultiByte(CP_UTF8, 0, str, static_cast<int>(len), &result[0], bytes, NULL, NULL))
                throw std::runtime_error(error);

            return result;
        }
        operator std::string() const {
            return to_utf8();
        }

        LPCWSTR data() const {return str;}
        operator LPCWSTR() const {
            return str;
        }

        size_t size() const {return len;}
        size_t length() const {return len;}

        bool operator==(const ApiString &other) const {
            return len == other.len && memcmp(str, other.str, len * sizeof(*str)) == 0;
        }
        bool operator!=(const ApiString &other) const {
            return !operator==(other);
        }

    public:
        LPWSTR str;
        size_t len;
        DestroyType type;

        void destroy() {
            switch (type) {
                default: break;
                case ApiLocalFree: LocalFree(str); break;
                case ApiGlobalFree: GlobalFree(str); break;
                case ApiDeleteArray: delete[] str; break;
                case ApiFree: free(str); break;
            }
        }
    };

    inline ApiString system_error_string(DWORD system_error) {
        LPWSTR str = NULL;

        if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, system_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&str), 0, NULL)) {
            // For some reason, every message gets a \r\n appended to the end
            const size_t len = wcslen(str);
            if (len >= 2 && str[len-2] == '\r' && str[len-1] == '\n')
                str[len-2] = 0;

            return ApiString(str, ApiString::ApiLocalFree);
        }

        return {};
    }
}

// Handy reference for Windows Async programming:
// https://www.microsoftpressstore.com/articles/article.aspx?p=2224047&seqNum=5
// and http://www.kegel.com/c10k.html
#endif

#endif // SKATE_SYSTEM_INCLUDES_H