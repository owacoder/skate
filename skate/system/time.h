#ifndef SKATE_TIME_H
#define SKATE_TIME_H

#include <time.h>
#include <mutex>

#include "includes.h"

namespace skate {
    std::mutex &time_mutex() {
        // See https://stackoverflow.com/questions/185624/static-variables-in-an-inlined-function
        static std::mutex mtx;
        return mtx;
    }

    namespace impl {
        template<typename T>
        auto localtime_r_impl(const T *timer, struct tm *buf, int) -> typename std::decay<decltype(::localtime_r(timer, buf), buf)>::type {
            return ::localtime_r(timer, buf);
        }

        template<typename T>
        auto localtime_r_impl(const T *timer, struct tm *buf, char) -> typename std::decay<decltype(buf)>::type {
#if MSVC_COMPILER
            return localtime_s(buf, timer)? nullptr: buf;
#else
            std::lock_guard<std::mutex> lock(time_mutex());
            const struct tm *internal = localtime(timer);
            if (!internal)
                return nullptr;

            *buf = *internal;
            return buf;
#endif
        }

        template<typename T>
        auto gmtime_r_impl(const T *timer, struct tm *buf, int) -> typename std::decay<decltype(::gmtime_r(timer, buf), buf)>::type {
            return ::gmtime_r(timer, buf);
        }

        template<typename T>
        auto gmtime_r_impl(const T *timer, struct tm *buf, char) -> typename std::decay<decltype(buf)>::type {
#if MSVC_COMPILER
            return gmtime_s(buf, timer) ? nullptr : buf;
#else
            std::lock_guard<std::mutex> lock(time_mutex());
            const struct tm *internal = gmtime(timer);
            if (!internal)
                return nullptr;

            *buf = *internal;
            return buf;
#endif
        }
    }

    struct tm *localtime_r(time_t timer, struct tm &buf) {
        return impl::localtime_r_impl(&timer, &buf, 0);
    }

    struct tm *localtime_r(const time_t *timer, struct tm *buf) {
        return impl::localtime_r_impl(timer, buf, 0);
    }

    struct tm *gmtime_r(time_t timer, struct tm &buf) {
        return impl::gmtime_r_impl(&timer, &buf, 0);
    }

    struct tm *gmtime_r(const time_t *timer, struct tm *buf) {
        return impl::gmtime_r_impl(timer, buf, 0);
    }

    struct tm localtime(time_t timer) {
        tm result;
        if (!localtime_r(timer, result))
            return {};

        return result;
    }

    struct tm gmtime(time_t timer) {
        tm result;
        if (!gmtime_r(timer, result))
            return {};

        return result;
    }

    std::string strftime(const char *format, const struct tm &timeptr) {
        std::string result(strlen(format) + 128, '\0');

        while (true) {
            const size_t written = strftime(&result[0], result.size(), format, &timeptr);
            if (written) {
                result.resize(written);
                return result;
            }

            result.resize(result.size() + (result.size() >> 1));
        }
    }

    std::string asctime(const struct tm &timeptr) {
        static const char weekday[][4] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        static const char month[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        char buffer[26];

        snprintf(buffer, sizeof(buffer), "%s %s%3d %.2d:%.2d:%.2d %.4d",
                weekday[timeptr.tm_wday],
                month[timeptr.tm_mon],
                timeptr.tm_mday,
                timeptr.tm_hour,
                timeptr.tm_min,
                timeptr.tm_sec,
                timeptr.tm_year + 1900);

        return buffer;
    }

    std::string ctime(time_t timer) {
        return asctime(localtime(timer));
    }
}

#endif // SKATE_TIME_H
