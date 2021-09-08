/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_IO_ADAPTERS_CORE_H
#define SKATE_IO_ADAPTERS_CORE_H

#include <cmath>
#include <type_traits>
#include <limits>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <tuple>
#include <array>
#include <vector>
#include <map>

#if __cplusplus >= 201703L
# include <charconv>
#endif

#if __cplusplus >= 202002L
# include <format>
#endif

#include "../../containers/utf.h"
#include "../../containers/abstract_list.h"
#include "../../containers/split_join.h"

namespace skate {
    namespace impl {
        template<typename StreamChar>
        bool skipws(std::basic_streambuf<StreamChar> &is) {
            while (true) {
                const auto c = is.sgetc();

                if (c == std::char_traits<StreamChar>::eof()) // Already at end
                    return false;
                else if (!isspace(c)) // Not a space, good result
                    return true;
                else if (is.sbumpc() == std::char_traits<StreamChar>::eof()) // No next character
                    return false;
            }
        }

        template<typename StreamChar>
        bool skip_spaces_and_tabs(std::basic_streambuf<StreamChar> &is) {
            while (true) {
                const auto c = is.sgetc();

                if (c == std::char_traits<StreamChar>::eof()) // Already at end
                    return false;
                else if (!isspace_or_tab(c)) // Not a space, good result
                    return true;
                else if (is.sbumpc() == std::char_traits<StreamChar>::eof()) // No next character
                    return false;
            }
        }

        template<typename IntType>
        bool parse_int(const char *s, IntType &ref, int base = 10, bool only_number = false) {
            char *end = nullptr;

            while (isspace(*s))
                ++s;

            if (*s == '-') {
                errno = 0;
                const auto v = strtoll(s, &end, base);
                if (errno == ERANGE || v > std::numeric_limits<IntType>::max() || v < std::numeric_limits<IntType>::min())
                    return false;

                if (std::is_unsigned<IntType>::value && v < 0)
                    return false;

                ref = IntType(v);
            } else {
                if (*s == '+')
                    ++s;

                errno = 0;
                const auto v = strtoull(s, &end, base);
                if (errno == ERANGE || v > static_cast<typename std::make_unsigned<IntType>::type>(std::numeric_limits<IntType>::max()))
                    return false;

                ref = IntType(v);
            }

            if (only_number? *end != 0: end == s) { // If only_number, require entire string to be valid integer, otherwise just the beginning
                ref = 0;
                return false;
            }

            return true;
        }

        // Requires that number start with digit or '-'. Leading '+' is not allowed by default
        template<typename StreamChar, typename IntType>
        bool read_int(std::basic_streambuf<StreamChar> &is, IntType &ref, bool allow_leading_plus = false) {
            auto first = is.sgetc();
            if (!isdigit(first) && first != '-' && (!allow_leading_plus || first != '+'))
                return false;

            std::string temp;
            temp.push_back(char(first));

            while (true) {
                const auto c = is.snextc();

                if (c == std::char_traits<StreamChar>::eof() || !isdigit(c))
                    break;

                temp.push_back(char(c));
            }

            return parse_int(temp.c_str(), ref, 10, true);
        }

        template<typename FloatType>
        bool parse_float(const char *s, FloatType &ref, bool only_number = false) {
            char *end = nullptr;

            while (isspace(*s))
                ++s;

            if (std::is_same<typename std::decay<FloatType>::type, float>::value)
                ref = FloatType(strtof(s, &end));
            else if (std::is_same<typename std::decay<FloatType>::type, double>::value)
                ref = FloatType(strtod(s, &end));
            else
                ref = FloatType(strtold(s, &end));

            return only_number? *end == 0: end != s; // If only_number, require entire string to be valid floating point, otherwise just the beginning
        }

        // Requires that number start with digit or '-'. Leading '+' is not allowed by default
        template<typename StreamChar, typename FloatType>
        bool read_float(std::basic_streambuf<StreamChar> &is, FloatType &ref, bool allow_leading_dot = false, bool allow_leading_plus = false) {
            static_assert(std::is_same<FloatType, float>::value ||
                          std::is_same<FloatType, double>::value ||
                          std::is_same<FloatType, long double>::value, "floating point type must be float, double, or long double");

            ref = 0.0;
            auto first = is.sgetc();
            if (!isdigit(first) && first != '-' && (!allow_leading_dot || first != '.') && (!allow_leading_plus || first != '+'))
                return false;

            std::string temp;
            temp.push_back(char(first));

            while (true) {
                const auto c = is.snextc();

                if (c == std::char_traits<StreamChar>::eof() || !isfpdigit(c))
                    break;

                temp.push_back(char(c));
            }

            return parse_float(temp.c_str(), ref, true);
        }

        template<typename StreamChar, typename IntType>
        bool write_int(std::basic_streambuf<StreamChar> &os, IntType v) {
#if __cplusplus >= 201703L
            std::array<char, std::numeric_limits<IntType>::digits10 + 1 + std::is_signed<IntType>::value> buf;

            const auto result = std::to_chars(buf.data(), buf.data() + buf.size(), v);
            if (result.ec != std::errc())
                return false;
            const auto end = result.ptr;
#else
            const std::string buf = std::to_string(v);
            const auto end = buf.data() + buf.size();
#endif

            for (auto begin = buf.data(); begin != end; ++begin)
                if (os.sputc(*begin) == std::char_traits<StreamChar>::eof())
                    return false;

            return true;
        }

        template<typename StreamChar, typename IntType>
        bool read_little_endian(std::basic_streambuf<StreamChar> &is, IntType &v) {
            typedef typename std::make_unsigned<IntType>::type UIntType;
            UIntType copy = 0;

            v = 0;
            for (size_t i = 0; i < std::numeric_limits<UIntType>::digits / 8; ++i) {
                const auto c = is.sbumpc();
                if (c == std::char_traits<StreamChar>::eof())
                    return false;

                copy |= UIntType(c & 0xff) << (i * 8);
            }

            v = copy;
            return true;
        }

        template<typename StreamChar, typename IntType>
        bool read_big_endian(std::basic_streambuf<StreamChar> &is, IntType &v) {
            typedef typename std::make_unsigned<IntType>::type UIntType;
            UIntType copy = 0;

            v = 0;
            for (size_t i = 0; i < std::numeric_limits<UIntType>::digits / 8; ++i) {
                const auto c = is.sbumpc();
                if (c == std::char_traits<StreamChar>::eof())
                    return false;

                copy = (copy << 8) | (c & 0xff);
            }

            v = copy;
            return true;
        }

        template<typename StreamChar, typename IntType>
        bool write_little_endian(std::basic_streambuf<StreamChar> &os, IntType v) {
            typedef typename std::make_unsigned<IntType>::type UIntType;
            const UIntType copy = v;

            for (size_t i = 0; i < std::numeric_limits<UIntType>::digits / 8; ++i)
                if (os.sputc((copy >> (i * 8)) & 0xff) == std::char_traits<StreamChar>::eof())
                    return false;

            return true;
        }

        template<typename StreamChar, typename IntType>
        bool write_big_endian(std::basic_streambuf<StreamChar> &os, IntType v) {
            typedef typename std::make_unsigned<IntType>::type UIntType;
            const UIntType copy = v;

            for (size_t i = std::numeric_limits<UIntType>::digits / 8; i; --i)
                if (os.sputc((copy >> ((i - 1) * 8)) & 0xff) == std::char_traits<StreamChar>::eof())
                    return false;

            return true;
        }

        namespace impl {
            template<typename T>
            constexpr int log10ceil(T num) {
                return num < 10? 1: 1 + log10ceil(num / 10);
            }

            // Needed because std::max isn't constexpr
            template<typename T>
            constexpr T max(T a, T b) { return a < b? b: a; }
        }

        template<typename StreamChar, typename FloatType>
        bool write_float(std::basic_streambuf<StreamChar> &os, FloatType v, bool allow_inf = true, bool allow_nan = true) {
            static_assert(std::is_same<typename std::decay<FloatType>::type, float>::value ||
                          std::is_same<typename std::decay<FloatType>::type, double>::value ||
                          std::is_same<typename std::decay<FloatType>::type, long double>::value, "floating point type must be float, double, or long double");

            if (std::isinf(v)) {
                if (!allow_inf)
                    return false;

                // Add negative sign as needed
                if (std::signbit(v) && os.sputc('-') == std::char_traits<StreamChar>::eof())
                    return false;

                const char buf[] = "Infinity";
                for (const auto c: buf)
                    if (os.sputc(c) == std::char_traits<StreamChar>::eof())
                        return false;

                return true;
            } else if (std::isnan(v)) {
                if (!allow_nan)
                    return false;

                const char buf[] = "NaN";
                for (const auto c: buf)
                    if (os.sputc(c) == std::char_traits<StreamChar>::eof())
                        return false;

                return true;
            }

            // See https://stackoverflow.com/questions/68472720/stdto-chars-minimal-floating-point-buffer-size/68475665#68475665
            // The buffer is guaranteed to be a minimally sized buffer for the output string
            std::array<char, 4 +
                             std::numeric_limits<FloatType>::max_digits10 +
                             impl::max(2, impl::log10ceil(std::numeric_limits<FloatType>::max_exponent10)) +
                             1 // Add for NUL terminator
                      > buf;

#if 0 && __cplusplus >= 201703L
            const auto result = std::to_chars(buf.data(), buf.data() + buf.size(), v, std::chars_format::general, std::numeric_limits<FloatType>::max_digits10);
            if (result.ec != std::errc())
                return false;

            for (auto begin = buf.data(); begin != result.ptr; ++begin)
                if (os.sputc(*begin) == std::char_traits<StreamChar>::eof())
                    return false;

            return true;
#else
            // Ideally this would not use snprintf, but there's not really a great (fast) option in C++11
            auto chars = snprintf(buf.data(), buf.size(),
                                  std::is_same<long double, FloatType>::value? "%.*Lg": "%.*g",
                                  std::numeric_limits<FloatType>::max_digits10, v);
            if (chars < 0)
                return false;

            for (size_t i = 0; i < size_t(chars); ++i)
                if (os.sputc((unsigned char) buf[i]) == std::char_traits<StreamChar>::eof())
                    return false;

            return true;
#endif
        }

        // Allows using apply() on a tuple object
        template<typename F, size_t size_of_tuple>
        class tuple_apply : private tuple_apply<F, size_of_tuple - 1> {
        public:
            template<typename Tuple>
            tuple_apply(Tuple &&t, F f) : tuple_apply<F, size_of_tuple - 1>(std::forward<Tuple>(t), f) {
                f(std::get<size_of_tuple - 1>(std::forward<Tuple>(t)));
            }
        };

        template<typename F>
        class tuple_apply<F, 0> {
        public:
            template<typename Tuple>
            constexpr tuple_apply(Tuple &&, F) noexcept {}
        };

        template<typename F, typename Tuple>
        void apply(F f, Tuple &&tuple) {
            tuple_apply<F, std::tuple_size<typename std::decay<Tuple>::type>::value>(std::forward<Tuple>(tuple), f);
        }
    }
}

// Qt helpers
#ifdef QT_VERSION
    namespace skate {
        template<>
        struct is_string<QByteArray> : public std::true_type {};

        template<>
        struct is_string<QString> : public std::true_type {};
    }
#endif

#endif // SKATE_IO_ADAPTERS_CORE_H
