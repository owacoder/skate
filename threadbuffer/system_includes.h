#ifndef SKATE_SYSTEM_INCLUDES_H
#define SKATE_SYSTEM_INCLUDES_H

#include "environment.h"

#include <stdexcept>

#if POSIX_OS
# include <unistd.h>

namespace Skate {
    // Really no difference between FileDescriptor and SocketDescriptor on Unix/POSIX systems
    typedef int FileDescriptor;
    typedef int SocketDescriptor;

    static constexpr int ErrorTimedOut = ETIMEDOUT;

    // Startup wrapper for Skate, requires a single object to be defined either statically, or at the start of main()
    class StartupWrapper {};

    class ApiString {
    public:
        enum DestroyType {
            ApiNoFree,
            ApiDeleteArray,
            ApiFree
        };

        ApiString() : str(""), len(0), type(ApiNoFree) {}
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
        char *buf = malloc(buf_size);

        do {
            if (strerror_r(system_error, buf, buf_size) == 0)
                break;

            if (errno != ERANGE) {
                free(buf);
                return {};
            }

            buf_size *= 2;
            char *new_buf = realloc(buf, buf_size);
            if (new_buf == nullptr) {
                free(buf);
                return {};
            }

            buf = new_buf;
        } while (1);

        return ApiString(buf, ApiString::ApiFree);
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
            const int chars = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, utf8, -1, NULL, 0);
            if (!chars)
                throw std::runtime_error("Cannot create ApiString from UTF-8");

            LPWSTR result = new WCHAR[chars];

            MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, utf8, -1, result, chars);

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
            destroy();

            str = other.str; other.str = nullptr;
            len = other.len; other.len = 0;
            type = other.type; other.type = ApiNoFree;

            return *this;
        }

        static ApiString from_utf8(const std::string &utf8) {return ApiString(utf8);}
        static ApiString from_utf8(const char *utf8) {return ApiString(utf8);}

        std::string to_utf8() const {
            std::string result;
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
            if (!bytes)
                throw std::runtime_error("Cannot convert ApiString to UTF-8");

            // Remove NUL-terminator on end by subtracting one from bytes
            result.resize(bytes-1);
            WideCharToMultiByte(CP_UTF8, 0, str, -1, &result[0], bytes-1, NULL, NULL);

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

    inline ApiString system_error_string(int system_error) {
        LPWSTR str = NULL;

        if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, system_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&str), 0, NULL))
            return ApiString(str, ApiString::ApiLocalFree);

        return {};
    }
}

// Handy reference for Windows Async programming:
// https://www.microsoftpressstore.com/articles/article.aspx?p=2224047&seqNum=5
// and http://www.kegel.com/c10k.html
#endif

#endif // SKATE_SYSTEM_INCLUDES_H
