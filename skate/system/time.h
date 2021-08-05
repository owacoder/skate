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

    struct time_point_string_options {
        constexpr time_point_string_options() : time_point_string_options(0u) {}
        constexpr time_point_string_options(unsigned fractional_second_places, bool utc = false, const char *format = "%F %T")
            : fractional_second_places(fractional_second_places)
            , utc(utc)
            , format(format)
        {}
        constexpr time_point_string_options(const char *format, unsigned fractional_second_places = 6, bool utc = false)
            : fractional_second_places(fractional_second_places)
            , utc(utc)
            , format(format)
        {}

        unsigned fractional_second_places;
        bool utc;
        const char *format;
    };

    static std::string time_point_to_string(std::chrono::time_point<std::chrono::system_clock> when, time_point_string_options options = {}) {
        const auto adjusted = std::chrono::time_point_cast<std::chrono::nanoseconds>(when);
        const auto truncated = adjusted - std::chrono::nanoseconds(adjusted.time_since_epoch().count() % 1000000000); // Truncate nanoseconds off adjusted time
        const time_t t = std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(truncated));
        struct tm x;

        if (options.utc)
            skate::gmtime_r(&t, &x);
        else
            skate::localtime_r(&t, &x);

        std::string result = skate::strftime(options.format, x);

        if (options.fractional_second_places) {
            std::string fractional = std::to_string(adjusted.time_since_epoch().count() % 1000000000);

            result.push_back('.');

            if (options.fractional_second_places < fractional.size())
                fractional.resize(options.fractional_second_places, '0');
            else
                result.resize(result.size() + options.fractional_second_places - fractional.size(), '0');

            result += fractional;
        }

        return result;
    }
}

#endif // SKATE_TIME_H
