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
#include <memory>

#include <tuple>
#include <array>
#include <vector>
#include <map>

#if __cplusplus >= 201703L
# include <optional>
# include <variant>
# include <charconv>
#endif

#if __cplusplus >= 202002L
# include <format>
#endif

#include "../../system/utf.h"
#include "../../system/includes.h"

namespace skate {
    // Determine if type is a string
    template<typename T> struct is_string : public std::false_type {};
    template<typename... ContainerParams>
    struct is_string<std::basic_string<ContainerParams...>> : public std::true_type {};
    template<>
    struct is_string<char *> : public std::true_type {};
    template<size_t N>
    struct is_string<char [N]> : public std::true_type {};
    template<>
    struct is_string<wchar_t *> : public std::true_type {};
    template<size_t N>
    struct is_string<wchar_t [N]> : public std::true_type {};
    template<>
    struct is_string<char16_t *> : public std::true_type {};
    template<size_t N>
    struct is_string<char16_t [N]> : public std::true_type {};
    template<>
    struct is_string<char32_t *> : public std::true_type {};
    template<size_t N>
    struct is_string<char32_t [N]> : public std::true_type {};
#if __cplusplus >= 201703L
    template<typename... ContainerParams>
    struct is_string<std::basic_string_view<ContainerParams...>> : public std::true_type {};
#endif
#if __cplusplus >= 202002L
    template<>
    struct is_string<char8_t *> : public std::true_type {};
    template<size_t N>
    struct is_string<char8_t [N]> : public std::true_type {};
#endif

    // If base type is pointer, strip const/volatile off pointed-to type
    template<typename T>
    struct is_string_base_helper : public is_string<T> {};
    template<typename T>
    struct is_string_base_helper<T *> : public is_string<typename std::remove_cv<T>::type *> {};

    // Strip const/volatile off type
    template<typename T>
    struct is_string_base : public is_string_base_helper<typename std::decay<T>::type> {};

    // Determine if type is a map pair (has first/second members, or key()/value() functions)
    template<typename MapPair>
    struct is_map_pair_helper {
        struct none {};

        // Test for indirect first/second pair
        template<typename U, typename _ = MapPair> static std::pair<decltype(std::declval<_>()->first), decltype(std::declval<_>()->second)> ind_first_second(U *);
        template<typename U> static none ind_first_second(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>()->first) ind_first(U *);
        template<typename U> static none ind_first(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>()->second) ind_second(U *);
        template<typename U> static none ind_second(...);

        // Test for member first/second pair
        template<typename U, typename _ = MapPair> static std::pair<decltype(std::declval<_>().first), decltype(std::declval<_>().second)> mem_first_second(U *);
        template<typename U> static none mem_first_second(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>().first) mem_first(U *);
        template<typename U> static none mem_first(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>().second) mem_second(U *);
        template<typename U> static none mem_second(...);

        // Test for indirect key()/value() functions
        template<typename U, typename _ = MapPair> static std::pair<decltype(std::declval<_>()->key()), decltype(std::declval<_>()->value())> ind_key_value(U *);
        template<typename U> static none ind_key_value(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>()->key()) ind_key(U *);
        template<typename U> static none ind_key(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>()->value()) ind_value(U *);
        template<typename U> static none ind_value(...);

        // Test for member key()/value() functions
        template<typename U, typename _ = MapPair> static std::pair<decltype(std::declval<_>().key()), decltype(std::declval<_>().value())> mem_key_value(U *);
        template<typename U> static none mem_key_value(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>().key()) mem_key(U *);
        template<typename U> static none mem_key(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>().value()) mem_value(U *);
        template<typename U> static none mem_value(...);

        static constexpr int value = !std::is_same<none, decltype(ind_first_second<MapPair>(nullptr))>::value ||
                                     !std::is_same<none, decltype(mem_first_second<MapPair>(nullptr))>::value ||
                                     !std::is_same<none, decltype(ind_key_value<MapPair>(nullptr))>::value ||
                                     !std::is_same<none, decltype(mem_key_value<MapPair>(nullptr))>::value;

        typedef typename std::conditional<!std::is_same<none, decltype(ind_first<MapPair>(nullptr))>::value, decltype(ind_first<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(mem_first<MapPair>(nullptr))>::value, decltype(mem_first<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(ind_key<MapPair>(nullptr))>::value, decltype(ind_key<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(mem_key<MapPair>(nullptr))>::value, decltype(mem_key<MapPair>(nullptr)), void>::type>::type>::type>::type key_type;

        typedef typename std::conditional<!std::is_same<none, decltype(ind_second<MapPair>(nullptr))>::value, decltype(ind_second<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(mem_second<MapPair>(nullptr))>::value, decltype(mem_second<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(ind_value<MapPair>(nullptr))>::value, decltype(ind_value<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(mem_value<MapPair>(nullptr))>::value, decltype(mem_value<MapPair>(nullptr)), void>::type>::type>::type>::type value_type;
    };

    // Determine if type is an array (has begin() overload)
    template<typename Array>
    struct is_array_helper {
        struct none {};

        template<typename U, typename _ = Array> static typename std::enable_if<type_exists<decltype(begin(std::declval<_>()))>::value, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Array>(nullptr))>::value;
    };

    // Determine if type is a map (iterator has first/second members, or key()/value() functions)
    template<typename Map>
    struct is_map_helper {
        struct none {};

        template<typename U, typename _ = Map> static typename std::enable_if<is_map_pair_helper<decltype(begin(std::declval<_>()))>::value, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Map>(nullptr))>::value;
    };

    template<typename Map>
    struct is_string_map_helper {
        struct none {};

        template<typename U, typename _ = Map> static typename std::enable_if<is_string_base<typename is_map_pair_helper<decltype(begin(std::declval<_>()))>::key_type>::value, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Map>(nullptr))>::value;
    };

    template<typename MapPair, typename = typename is_map_pair_helper<MapPair>::key_type> struct key_of;

    template<typename MapPair>
    struct key_of<MapPair, decltype(std::declval<MapPair>()->first)> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>()->first) &operator()(const _ &m) const { return m->first; }
    };

    template<typename MapPair>
    struct key_of<MapPair, decltype(std::declval<MapPair>().first)> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>().first) &operator()(const _ &m) const { return m.first; }
    };

    template<typename MapPair>
    struct key_of<MapPair, decltype(std::declval<MapPair>()->key())> {
        template<typename _ = MapPair>
        decltype(std::declval<MapPair>()->key()) operator()(const _ &m) const { return m->key(); }
    };

    template<typename MapPair>
    struct key_of<MapPair, decltype(std::declval<MapPair>().key())> {
        template<typename _ = MapPair>
        decltype(std::declval<MapPair>().key()) operator()(const _ &m) const { return m.key(); }
    };

    template<typename MapPair, typename = typename is_map_pair_helper<MapPair>::value_type> struct value_of;

    template<typename MapPair>
    struct value_of<MapPair, decltype(std::declval<MapPair>()->second)> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>()->second) &operator()(const _ &m) const { return m->second; }
    };

    template<typename MapPair>
    struct value_of<MapPair, decltype(std::declval<MapPair>().second)> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>().second) &operator()(const _ &m) const { return m.second; }
    };

    template<typename MapPair>
    struct value_of<MapPair, decltype(std::declval<MapPair>()->value())> {
        template<typename _ = MapPair>
        decltype(std::declval<MapPair>()->value()) operator()(const _ &m) const { return m->value(); }
    };

    template<typename MapPair>
    struct value_of<MapPair, decltype(std::declval<MapPair>().value())> {
        template<typename _ = MapPair>
        decltype(std::declval<MapPair>().value()) operator()(const _ &m) const { return m.value(); }
    };

    template<typename T> struct is_map : public std::false_type {};

    // Strip const/volatile off type and determine if it's a map
    template<typename T>
    struct is_map_base : public std::integral_constant<bool, is_map<typename std::decay<T>::type>::value ||
                                                             is_map_helper<typename std::decay<T>::type>::value> {};

    template<typename T>
    struct is_string_map_base : public std::integral_constant<bool, is_map_base<T>::value &&
                                                                    is_string_map_helper<T>::value> {};

    template<typename T> struct is_array : public std::false_type {};

    // Strip const/volatile off type and determine if it's an array
    template<typename T>
    struct is_array_base : public std::integral_constant<bool, (is_array<typename std::decay<T>::type>::value ||
                                                                is_array_helper<typename std::decay<T>::type>::value) &&
                                                                !is_map_base<T>::value &&
                                                                !is_string_base<T>::value> {};

    // Determine if type is tuple
    template<typename T> struct is_tuple : public std::false_type {};
    template<typename Tuple>
    struct is_tuple_helper {
        struct none {};

        template<typename U, typename _ = Tuple> static typename std::enable_if<std::tuple_size<_>::value >= 0, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Tuple>(nullptr))>::value;
    };


    template<typename T>
    struct is_tuple_base : public std::integral_constant<bool, is_tuple<typename std::decay<T>::type>::value ||
                                                               is_tuple_helper<typename std::decay<T>::type>::value> {};

    // Determine if type is trivial (not a tuple, array, or map)
    template<typename T>
    struct is_scalar_base : public std::integral_constant<bool, !is_tuple_base<T>::value &&
                                                                !is_array_base<T>::value &&
                                                                !is_map_base<T>::value> {};

    // Determine if type is tuple with trivial elements
    template<typename T = void, typename... Types>
    struct is_trivial_tuple_helper : public std::integral_constant<bool, is_scalar_base<T>::value &&
                                                                         is_trivial_tuple_helper<Types...>::value> {};

    template<typename T>
    struct is_trivial_tuple_helper<T> : public std::integral_constant<bool, is_scalar_base<T>::value> {};

    template<typename T>
    struct is_trivial_tuple_helper2 : public std::false_type {};
    template<template<typename...> class Tuple, typename... Types>
    struct is_trivial_tuple_helper2<Tuple<Types...>> : public std::integral_constant<bool, is_tuple_base<Tuple<Types...>>::value &&
                                                                                          is_trivial_tuple_helper<Types...>::value> {};

    template<typename T>
    struct is_trivial_tuple_base : public is_trivial_tuple_helper2<typename std::decay<T>::type> {};

    // Determine if type is unique_ptr
    template<typename T> struct is_unique_ptr : public std::false_type {};
    template<typename T, typename... ContainerParams> struct is_unique_ptr<std::unique_ptr<T, ContainerParams...>> : public std::true_type {};

    template<typename T> struct is_unique_ptr_base : public is_unique_ptr<typename std::decay<T>::type> {};

    // Determine if type is shared_ptr
    template<typename T> struct is_shared_ptr : public std::false_type {};
    template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : public std::true_type {};

    template<typename T> struct is_shared_ptr_base : public is_shared_ptr<typename std::decay<T>::type> {};

    // Determine if type is weak_ptr
    template<typename T> struct is_weak_ptr : public std::false_type {};
    template<typename T> struct is_weak_ptr<std::weak_ptr<T>> : public std::true_type {};

    template<typename T> struct is_weak_ptr_base : public is_weak_ptr<typename std::decay<T>::type> {};

#if __cplusplus >= 201703L
    // Determine if type is an optional
    template<typename T> struct is_optional : public std::false_type {};
    template<typename T> struct is_optional<std::optional<T>> : public std::true_type {};

    template<typename T> struct is_optional_base : public is_optional<typename std::decay<T>::type> {};
#endif

#if __cplusplus >= 202002L
    // Determine if type is a variant
    template<typename T> struct is_variant : public std::false_type {};
    template<typename... Args> struct is_variant<std::variant<Args...>> : public std::true_type {};

    template<typename T> struct is_variant_base : public is_variant<typename std::decay<T>::type> {};
#endif

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
            auto c = is.sgetc();
            if (!isdigit(c) && c != '-' && (!allow_leading_plus || c != '+'))
                return false;

            std::string temp;
            temp.push_back(char(c));

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

            if (std::is_same<FloatType, float>::value)
                ref = strtof(s, &end);
            else if (std::is_same<FloatType, double>::value)
                ref = strtod(s, &end);
            else
                ref = strtold(s, &end);

            return only_number? *end == 0: end != s; // If only_number, require entire string to be valid floating point, otherwise just the beginning
        }

        // Requires that number start with digit or '-'. Leading '+' is not allowed by default
        template<typename StreamChar, typename FloatType>
        bool read_float(std::basic_streambuf<StreamChar> &is, FloatType &ref, bool allow_leading_dot = false, bool allow_leading_plus = false) {
            static_assert(std::is_same<FloatType, float>::value ||
                          std::is_same<FloatType, double>::value ||
                          std::is_same<FloatType, long double>::value, "floating point type must be float, double, or long double");

            ref = 0.0;
            auto c = is.sgetc();
            if (!isdigit(c) && c != '-' && (!allow_leading_dot || c != '.') && (!allow_leading_plus || c != '+'))
                return false;

            std::string temp;
            temp.push_back(char(c));

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
