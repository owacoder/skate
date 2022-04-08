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
#include <cassert>

#include <tuple>
#include <array>
#include <vector>
#include <map>

#if __cplusplus >= 201703L
# include <charconv>
# include <optional>
#endif

#if __cplusplus >= 202002L
# include <format>
#endif

#include "../../containers/utf.h"
#include "../../containers/abstract_map.h"

namespace skate {
    template<typename InputIterator>
    InputIterator skip_spaces_or_tabs(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            switch (std::uint32_t(first))
            if (*first != ' ' &&
                *first != '\t')
                return first;
        }

        return first;
    }

    template<typename InputIterator>
    InputIterator skip_whitespace(InputIterator first, InputIterator last) {
        for (; first != last; ++first)
            if (*first != ' ' &&
                *first != '\t' &&
                *first != '\n' &&
                *first != '\r')
                return first;

        return first;
    }

    template<typename InputIterator>
    input_result<InputIterator> starts_with(InputIterator first, InputIterator last, char c) {
        const bool matches = first != last ? *first == c : false;
        if (!matches)
            return { first, result_type::failure };

        return { ++first, result_type::success };
    }

    template<typename InputIterator>
    input_result<InputIterator> istarts_with(InputIterator first, InputIterator last, char c) {
        const bool matches = first != last ? tolower(*first) == tolower(c) : false;
        if (!matches)
            return { first, result_type::failure };

        return { ++first, result_type::success };
    }

    template<typename InputIterator>
    input_result<InputIterator> starts_with(InputIterator first, InputIterator last, const char *s) {
        for (; first != last && *s; ++first, ++s) {
            if (*first != *s)
                return { first, result_type::failure };
        }

        return { first, result_type::success };
    }

    template<typename InputIterator>
    input_result<InputIterator> istarts_with(InputIterator first, InputIterator last, const char *s) {
        for (; first != last && *s; ++first, ++s) {
            if (tolower(*first) != tolower(*s))
                return { first, result_type::failure };
        }

        return { first, result_type::success };
    }

    template<typename T, typename InputIterator>
    parsing_result<InputIterator, T> little_endian_decode_next(InputIterator first, InputIterator last) {
        using DecayedT = typename std::decay<T>::type;
        static_assert(std::is_unsigned<DecayedT>::value, "Only unsigned integer types can be parsed");
        static_assert(std::numeric_limits<DecayedT>::digits % 8 == 0, "Only integral types that are a multiple of 8 bits can be parsed");

        DecayedT value = 0;

        for (std::size_t i = 0; i < std::numeric_limits<DecayedT>::digits; i += 8, ++first) {
            if (first == last)
                return { first, value, result_type::failure };

            value |= DecayedT(std::uint8_t(*first)) << i;
        }

        return { first, value, result_type::success };
    }

    template<typename T, typename InputIterator, typename OutputIterator>
    parsing_result<InputIterator, OutputIterator> little_endian_decode(InputIterator first, InputIterator last, OutputIterator out) {
        result_type result = result_type::success;

        while (first != last) {
            typename std::decay<T>::type value = 0;

            std::tie(first, value, result) = little_endian_decode_next(first, last);
            if (result != result_type::success)
                break;

            *out++ = value;
        }

        return { first, out, result };
    }

    template<typename T, typename InputIterator>
    parsing_result<InputIterator, T> big_endian_decode_next(InputIterator first, InputIterator last) {
        using DecayedT = typename std::decay<T>::type;
        static_assert(std::is_unsigned<DecayedT>::value, "Only unsigned integer types can be parsed");
        static_assert(std::numeric_limits<DecayedT>::digits % 8 == 0, "Only integral types that are a multiple of 8 bits can be parsed");

        DecayedT value = 0;

        for (std::size_t i = std::numeric_limits<DecayedT>::digits; i > 0; i -= 8) {
            if (first == last) {
                return { first, value << (i % std::numeric_limits<DecayedT>::digits), result_type::failure };
            } else {
                value = (value << 8) | std::uint8_t(*first++);
            }
        }

        return { first, value, result_type::success };
    }

    template<typename T, typename InputIterator, typename OutputIterator>
    parsing_result<InputIterator, OutputIterator> big_endian_decode(InputIterator first, InputIterator last, OutputIterator out) {
        result_type result = result_type::success;

        while (first != last) {
            typename std::decay<T>::type value = 0;

            std::tie(first, value, result) = big_endian_decode_next(first, last);
            if (result != result_type::success)
                break;

            *out++ = value;
        }

        return { first, out, result };
    }

    template<typename T, typename OutputIterator>
    OutputIterator little_endian_encode(T &&value, OutputIterator out) {
        using DecayedT = typename std::decay<T>::type;
        static_assert(std::is_unsigned<DecayedT>::value, "Only unsigned integer types can be serialized");
        static_assert(std::numeric_limits<DecayedT>::digits % 8 == 0, "Only integral types that are a multiple of 8 bits can be serialized");

        for (std::size_t i = 0; i < std::numeric_limits<DecayedT>::digits; i += 8)
            *out++ = std::uint8_t(value >> i);

        return out;
    }

    template<typename InputIterator, typename OutputIterator>
    OutputIterator little_endian_encode(InputIterator first, InputIterator last, OutputIterator out) {
        for (; first != last; ++first)
            out = little_endian_encode(*first, out);

        return out;
    }

    template<typename T, typename OutputIterator>
    OutputIterator big_endian_encode(T &&value, OutputIterator out) {
        using DecayedT = typename std::decay<T>::type;
        static_assert(std::is_unsigned<DecayedT>::value, "Only unsigned integer types can be serialized");
        static_assert(std::numeric_limits<DecayedT>::digits % 8 == 0, "Only integral types that are a multiple of 8 bits can be serialized");

        for (std::size_t i = std::numeric_limits<DecayedT>::digits; i > 0; )
            *out++ = std::uint8_t(value >> (i -= 8));

        return out;
    }

    template<typename InputIterator, typename OutputIterator>
    OutputIterator big_endian_encode(InputIterator first, InputIterator last, OutputIterator out) {
        for (; first != last; ++first)
            out = big_endian_encode(*first, out);

        return out;
    }

    template<typename OutputIterator>
    class little_endian_encode_iterator {
        OutputIterator m_out;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr little_endian_encode_iterator(OutputIterator out) : m_out(out) {}
        constexpr little_endian_encode_iterator(const little_endian_encode_iterator &) = default;

        constexpr little_endian_encode_iterator &operator=(const little_endian_encode_iterator &) = default;
        template<typename T, typename std::enable_if<std::is_unsigned<typename std::decay<T>::type>::value, int>::type = 0>
        constexpr little_endian_encode_iterator &operator=(T &&value) { return m_out = little_endian_encode(std::forward<T>(value), m_out), *this; }

        constexpr little_endian_encode_iterator &operator*() noexcept { return *this; }
        constexpr little_endian_encode_iterator &operator++() noexcept { return *this; }
        constexpr little_endian_encode_iterator &operator++(int) noexcept { return *this; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename OutputIterator>
    class big_endian_encode_iterator {
        OutputIterator m_out;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr big_endian_encode_iterator(OutputIterator out) : m_out(out) {}
        constexpr big_endian_encode_iterator(const big_endian_encode_iterator &) = default;

        constexpr big_endian_encode_iterator &operator=(const big_endian_encode_iterator &) = default;
        template<typename T, typename std::enable_if<std::is_unsigned<typename std::decay<T>::type>::value, int>::type = 0>
        constexpr big_endian_encode_iterator &operator=(T &&value) { return m_out = big_endian_encode(std::forward<T>(value), m_out), *this; }

        constexpr big_endian_encode_iterator &operator*() noexcept { return *this; }
        constexpr big_endian_encode_iterator &operator++() noexcept { return *this; }
        constexpr big_endian_encode_iterator &operator++(int) noexcept { return *this; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename OutputIterator>
    OutputIterator hex_encode(std::uint8_t byte_value, OutputIterator out) {
        *out++ = nibble_to_hex(byte_value >> 4);
        *out++ = nibble_to_hex(byte_value & 0xf);

        return out;
    }

    template<typename OutputIterator>
    OutputIterator hex_encode_lower(std::uint8_t byte_value, OutputIterator out) {
        *out++ = nibble_to_hex_lower(byte_value >> 4);
        *out++ = nibble_to_hex_lower(byte_value & 0xf);

        return out;
    }

    template<typename InputIterator, typename OutputIterator>
    OutputIterator hex_encode(InputIterator first, InputIterator last, OutputIterator out) {
        for (; first != last; ++first)
            out = hex_encode(*first, out);

        return out;
    }

    template<typename InputIterator, typename OutputIterator>
    OutputIterator hex_encode_lower(InputIterator first, InputIterator last, OutputIterator out) {
        for (; first != last; ++first)
            out = hex_encode_lower(*first, out);

        return out;
    }

    template<typename Container = std::string, typename InputIterator>
    Container to_hex(InputIterator first, InputIterator last) {
        Container result;

        skate::reserve(result, 2 * skate::size_to_reserve(first, last));

        hex_encode(first, last, skate::make_back_inserter(result));

        return result;
    }

    template<typename Container = std::string, typename Range>
    constexpr Container to_hex(const Range &range) { return to_hex<Container>(begin(range), end(range)); }

    template<typename Container = std::string, typename InputIterator>
    Container to_hex_lower(InputIterator first, InputIterator last) {
        Container result;

        skate::reserve(result, 2 * skate::size_to_reserve(first, last));

        hex_encode_lower(first, last, skate::make_back_inserter(result));

        return result;
    }

    template<typename Container = std::string, typename Range>
    constexpr Container to_hex_lower(const Range &range) { return to_hex_lower<Container>(begin(range), end(range)); }

    template<typename OutputIterator>
    class hex_encode_iterator {
        OutputIterator m_out;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr hex_encode_iterator(OutputIterator out) : m_out(out) {}

        constexpr hex_encode_iterator &operator=(std::uint8_t value) { return m_out = hex_encode(value, m_out), *this; }

        constexpr hex_encode_iterator &operator*() noexcept { return *this; }
        constexpr hex_encode_iterator &operator++() noexcept { return *this; }
        constexpr hex_encode_iterator &operator++(int) noexcept { return *this; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename OutputIterator>
    class hex_encode_lower_iterator {
        OutputIterator m_out;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr hex_encode_lower_iterator(OutputIterator out) : m_out(out) {}

        constexpr hex_encode_lower_iterator &operator=(std::uint8_t value) { return m_out = hex_encode_lower(value, m_out), *this; }

        constexpr hex_encode_lower_iterator &operator*() noexcept { return *this; }
        constexpr hex_encode_lower_iterator &operator++() noexcept { return *this; }
        constexpr hex_encode_lower_iterator &operator++(int) noexcept { return *this; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

#if __cplusplus >= 201703L
    template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    input_result<const char *> int_decode(const char *first, const char *last, T &v, int base = 10) {
        const auto result = std::from_chars(first, last, v, base);

        return { result.ptr, result.ec == std::errc() ? result_type::success : result_type::failure };
    }
#endif

    template<typename T, typename InputIterator, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    input_result<InputIterator> int_decode(InputIterator first, InputIterator last, T &v, int base = 10) {
        assert(base >= 2 && base <= 36);

        if (first == last)
            return { first, result_type::failure };

        T temp = 0;
        bool did_read = false;
        bool range_error = false;

        if (*first == '-' && std::is_signed<T>::value) {
            ++first;

            for (; first != last; ++first) {
                const auto d = base36_to_int(*first);
                if (d >= base)
                    break;
                else if (range_error)
                    continue;

                did_read = true;

                if (temp < std::numeric_limits<T>::min() / base) {
                    range_error = true;
                    continue;
                }
                temp *= base;

                if (temp < std::numeric_limits<T>::min() + d) {
                    range_error = true;
                    continue;
                }
                temp -= d;
            }
        } else {
            for (; first != last; ++first) {
                const auto d = base36_to_int(*first);
                if (d >= base)
                    break;
                else if (range_error)
                    continue;

                did_read = true;

                if (temp > std::numeric_limits<T>::max() / base) {
                    range_error = true;
                    continue;
                }
                temp *= base;

                if (temp > std::numeric_limits<T>::max() - d) {
                    range_error = true;
                    continue;
                }
                temp += d;
            }
        }

        if (!range_error && did_read) {
            v = temp;
            return { first, result_type::success };
        } else {
            return { first, result_type::failure };
        }
    }

    template<typename T, typename OutputIterator, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    output_result<OutputIterator> int_encode(T v, OutputIterator out, int base = 10) {
        if (base < 2 || base > 36)
            return { out, result_type::failure };

        std::array<char, std::numeric_limits<T>::digits10 + 1 + std::is_signed<T>::value> buf;

#if __cplusplus >= 201703L
        const auto result = std::to_chars(buf.data(), buf.data() + buf.size(), v, base);
        if (result.ec != std::errc())
            return { out, result_type::failure };

        const auto begin = buf.data();
        const auto end = result.ptr;
#else
        char *end = buf.data() + buf.size();
        char *begin = end;

        if (v < 0) {
            *out++ = '-';

            do {
                *(--begin) = int_to_base36(std::uint8_t(-static_cast<std::make_signed<T>::type>(v % base)));
                v /= base;
            } while (v);
        } else {
            do {
                *(--begin) = int_to_base36(std::uint8_t(v % base));
                v /= base;
            } while (v);
        }
#endif

        return { std::copy(begin, end, out), result_type::success };
    }

    namespace detail {
        template<typename T>
        constexpr int log10ceil(T num) {
            return num < 10? 1: 1 + log10ceil(num / 10);
        }

        // Needed because std::max isn't constexpr
        template<typename T>
        constexpr T max(T a, T b) { return a < b? b: a; }
    }

#if MSVC_COMPILER && __cplusplus >= 201703L
    template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    constexpr input_result<const char *> fp_decode(const char *first, const char *last, T &v) {
        const auto result = std::from_chars(first, last, v);

        return { result.ptr, result.ec == std::errc() ? result_type::success : result_type::failure };
    }
#endif

    // TODO: This algorithm cannot be single-pass for floating point values due to 'e' handling for inputs like '1.12e+' (which should parse as 1.12) or similar
    // This can cause issues with validation of inputs that std::from_chars would accept just fine
    template<typename T, typename InputIterator, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    input_result<InputIterator> fp_decode(InputIterator first, InputIterator last, T &v) {
        if (first == last)
            return { first, result_type::failure };

        std::string tempstr;
        const bool negative = *first == '-';

        if (negative) {
            tempstr.push_back('-');

            if (++first == last)
                return { first, result_type::failure };
        }

        if ((*first < '0' || *first > '9') && *first != '.') {
            if (*first == 'n' || *first == 'N') { // Check for NaN
                const auto result = istarts_with(++first, last, "an");

                if (result.result == result_type::success)
                    v = negative ? -std::numeric_limits<T>::quiet_NaN() : std::numeric_limits<T>::quiet_NaN();

                return result;
            } else if (*first == 'i' || *first == 'I') { // Check for Infinity
                const auto result = istarts_with(++first, last, "nfinity");

                if (result.result == result_type::success)
                    v = negative ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();

                return result;
            }

            return { first, result_type::failure };
        }

        // Parse beginning numeric portion
        for (; first != last && (*first >= '0' && *first <= '9'); ++first) {
            tempstr.push_back(char(*first));
        }

        // Parse decimal and following numeric portion, if present
        if (first != last && *first == '.') {
            if (++first != last) {
                tempstr.push_back('.');

                for (; first != last && (*first >= '0' && *first <= '9'); ++first) {
                    tempstr.push_back(char(*first));
                }
            }
        }

        // Parse exponent, if present
        if (first != last && (*first == 'e' || *first == 'E')) {
            if (++first != last) {
                tempstr.push_back('e');

                if (*first == '+' || *first == '-') {
                    tempstr.push_back(char(*first));

                    if (++first == last)
                        return { first, result_type::failure };
                }

                if (*first < '0' || *first > '9')
                    return { first, result_type::failure };

                for (; first != last && (*first >= '0' && *first <= '9'); ++first) {
                    tempstr.push_back(char(*first));
                }
            }
        }

#if MSVC_COMPILER && __cplusplus >= 201703L
        const auto result = std::from_chars(tempstr.data(), tempstr.data() + tempstr.size(), v);

        return { first, result.ec == std::errc() && result.ptr == tempstr.data() + tempstr.size() ? result_type::success : result_type::failure };
#else
        T temp = T(0.0);
        char *end = nullptr;

        if (std::is_same<typename std::decay<T>::type, float>::value)
            temp = T(std::strtof(tempstr.c_str(), &end));
        else if (std::is_same<typename std::decay<T>::type, double>::value)
            temp = T(std::strtod(tempstr.c_str(), &end));
        else
            temp = T(std::strtold(tempstr.c_str(), &end));

        if (*end == 0) {
            v = temp;
            return { first, result_type::success };
        } else {
            return { first, result_type::failure };
        }
#endif
    }

    template<typename T, typename OutputIterator, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    output_result<OutputIterator> fp_encode(T v, OutputIterator out, bool allow_inf = true, bool allow_nan = true) {
        static_assert(std::is_same<typename std::decay<T>::type, float>::value ||
                      std::is_same<typename std::decay<T>::type, double>::value ||
                      std::is_same<typename std::decay<T>::type, long double>::value, "floating point type must be float, double, or long double");

        if (std::isinf(v)) {
            if (!allow_inf)
                return { out, result_type::failure };

            // Add negative sign as needed
            if (std::signbit(v))
                *out++ = '-';

            const char buf[] = "Infinity";

            return { std::copy_n(buf, sizeof(buf) - 1, out), result_type::success };
        } else if (std::isnan(v)) {
            if (!allow_nan)
                return { out, result_type::failure };

            const char buf[] = "NaN";

            return { std::copy_n(buf, sizeof(buf) - 1, out), result_type::success };
        }

        // See https://stackoverflow.com/questions/68472720/stdto-chars-minimal-floating-point-buffer-size/68475665#68475665
        // The buffer is guaranteed to be a minimally sized buffer for the output string
        std::array<char, 4 +
                         std::numeric_limits<T>::max_digits10 +
                         detail::max(2, detail::log10ceil(std::numeric_limits<T>::max_exponent10)) +
                         1 // Add for NUL terminator
                  > buf;

#if MSVC_COMPILER && __cplusplus >= 201703L
        const auto result = std::to_chars(buf.data(), buf.data() + buf.size(), v, std::chars_format::general, std::numeric_limits<T>::max_digits10);
        if (result.ec != std::errc())
            return { out, result_type::failure };

        return { std::copy(buf.data(), result.ptr, out), result_type::success };
#else
        // Ideally this would not use snprintf, but there's not really a great (fast) option in C++11
        auto chars = std::snprintf(buf.data(), buf.size(),
                                   std::is_same<long double, T>::value? "%.*Lg": "%.*g",
                                   std::numeric_limits<T>::max_digits10, v);
        if (chars < 0)
            return { out, result_type::failure };

        return { std::copy_n(buf.begin(), std::size_t(chars), out), result_type::success };
#endif
    }

    namespace detail {
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
    }

    template<typename F, typename Tuple>
    void apply(F f, Tuple &&tuple) {
        detail::tuple_apply<F, std::tuple_size<typename std::decay<Tuple>::type>::value>(std::forward<Tuple>(tuple), f);
    }
}

#endif // SKATE_IO_ADAPTERS_CORE_H
