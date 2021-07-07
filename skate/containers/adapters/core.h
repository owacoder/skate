#ifndef SKATE_ADAPTERS_H
#define SKATE_ADAPTERS_H

#include <cmath>
#include <type_traits>
#include <limits>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <memory>

#include <vector>
#include <map>

#if __cplusplus >= 201703L
# include <optional>
# include <variant>
#endif

#include "utf.h"

namespace skate {
    using std::begin;
    using std::end;

    template<typename T> struct type_exists : public std::true_type { typedef int type; };

    template<typename T> struct base_type {
        typedef typename std::remove_cv<typename std::remove_reference<T>::type>::type type;
    };

    // Determine if type is a string
    template<typename T> struct is_string : public std::false_type {};
    template<typename... ContainerParams>
    struct is_string<std::basic_string<ContainerParams...>> : public std::true_type {};
    template<>
    struct is_string<char *> : public std::true_type {};
    template<>
    struct is_string<wchar_t *> : public std::true_type {};
    template<>
    struct is_string<char16_t *> : public std::true_type {};
    template<>
    struct is_string<char32_t *> : public std::true_type {};
#if __cplusplus >= 201703L
    template<typename... ContainerParams>
    struct is_string<std::basic_string_view<ContainerParams...>> : public std::true_type {};
#endif
#if __cplusplus >= 202002L
    template<>
    struct is_string<char8_t *> : public std::true_type {};
#endif

    // If base type is pointer, strip const/volatile off pointed-to type
    template<typename T>
    struct is_string_base_helper : public is_string<T> {};
    template<typename T>
    struct is_string_base_helper<T *> : public is_string<typename std::remove_cv<T>::type *> {};

    // Strip const/volatile off type
    template<typename T>
    struct is_string_base : public is_string_base_helper<typename base_type<T>::type> {};

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
        const decltype(std::declval<MapPair>()->key()) &operator()(const _ &m) const { return m->key(); }
    };

    template<typename MapPair>
    struct key_of<MapPair, decltype(std::declval<MapPair>().key())> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>().key()) &operator()(const _ &m) const { return m.key(); }
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
        const decltype(std::declval<MapPair>()->value()) &operator()(const _ &m) const { return m->value(); }
    };

    template<typename MapPair>
    struct value_of<MapPair, decltype(std::declval<MapPair>().value())> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>().value()) &operator()(const _ &m) const { return m.value(); }
    };

    template<typename T> struct is_map : public std::false_type {};

    // Strip const/volatile off type
    template<typename T>
    struct is_map_base : public std::integral_constant<bool, is_map<typename base_type<T>::type>::value ||
                                                             is_map_helper<typename base_type<T>::type>::value> {};

    template<typename T>
    struct is_string_map_base : public std::integral_constant<bool, is_map_base<T>::value &&
                                                                    is_string_map_helper<T>::value> {};

    // Determine if type is unique_ptr
    template<typename T> struct is_unique_ptr : public std::false_type {};
    template<typename T, typename... ContainerParams> struct is_unique_ptr<std::unique_ptr<T, ContainerParams...>> : public std::true_type {};

    template<typename T> struct is_unique_ptr_base : public is_unique_ptr<typename base_type<T>::type> {};

    // Determine if type is shared_ptr
    template<typename T> struct is_shared_ptr : public std::false_type {};
    template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : public std::true_type {};

    template<typename T> struct is_shared_ptr_base : public is_shared_ptr<typename base_type<T>::type> {};

    // Determine if type is weak_ptr
    template<typename T> struct is_weak_ptr : public std::false_type {};
    template<typename T> struct is_weak_ptr<std::weak_ptr<T>> : public std::true_type {};

    template<typename T> struct is_weak_ptr_base : public is_weak_ptr<typename base_type<T>::type> {};

#if __cplusplus >= 201703L
    // Determine if type is an optional
    template<typename T> struct is_optional : public std::false_type {};
    template<typename T> struct is_optional<std::optional<T>> : public std::true_type {};

    template<typename T> struct is_optional_base : public is_optional<typename base_type<T>::type> {};
#endif

#if __cplusplus >= 202002L
    // Determine if type is a variant
    template<typename T> struct is_variant : public std::false_type {};
    template<typename... Args> struct is_variant<std::variant<Args...>> : public std::true_type {};

    template<typename T> struct is_variant_base : public is_variant<typename base_type<T>::type> {};
#endif

    namespace impl {
        template<typename CharType>
        constexpr int toxdigit(CharType t) {
            return (t >= '0' && t <= '9')? t - '0':
                   (t >= 'A' && t <= 'F')? (t - 'A' + 10):
                   (t >= 'a' && t <= 'f')? (t - 'a' + 10): -1;
        }

        template<typename CharType>
        std::basic_istream<CharType> &expect_char(std::basic_istream<CharType> &is, CharType expected) {
            CharType c;

            if (is.get(c) && c != expected)
                is.setstate(std::ios_base::failbit);

            return is;
        }

        template<typename CharType>
        constexpr bool isspace(CharType c) {
            return c == ' ' || c == '\n' || c == '\r' || c == '\t';
        }

        template<typename CharType>
        constexpr bool isdigit(CharType c) {
            return c >= '0' && c <= '9';
        }

        template<typename StreamChar>
        bool skipws(std::basic_streambuf<StreamChar> &is) {
            while (true) {
                const auto c = is.sgetc();

                if (c == std::char_traits<StreamChar>::eof()) // Already at end
                    return false;
                else if (!impl::isspace(c)) // Not a space, good result
                    return true;
                else if (is.sbumpc() == std::char_traits<StreamChar>::eof()) // No next character
                    return false;
            }
        }

        // Requires that number start with digit or '-'. Leading '+' is not allowed
        template<typename StreamChar, typename FloatType>
        bool read_float(std::basic_streambuf<StreamChar> &is, FloatType &ref, bool allow_leading_dot = false, bool allow_leading_plus = false) {
            static_assert(std::is_same<FloatType, float>::value ||
                          std::is_same<FloatType, double>::value ||
                          std::is_same<FloatType, long double>::value, "floating point type must be float, double, or long double");

            ref = 0.0;
            auto c = is.sgetc();
            if (!impl::isdigit(c) && c != '-' && (!allow_leading_dot || c != '.') && (!allow_leading_plus || c != '+'))
                return false;

            std::string temp;
            temp.push_back(char(c));

            while (true) {
                const auto c = is.snextc();

                if (c == std::char_traits<StreamChar>::eof() || !(impl::isdigit(c) || c == '-' || c == '.' || c == 'e' || c == 'E' || c == '+'))
                    break;

                temp.push_back(char(c));
            }

            char *end = nullptr;

            if (std::is_same<FloatType, float>::value)
                ref = strtof(temp.c_str(), &end);
            else if (std::is_same<FloatType, double>::value)
                ref = strtod(temp.c_str(), &end);
            else
                ref = strtold(temp.c_str(), &end);

            return *end == 0;
        }

        template<typename StreamChar, typename FloatType>
        bool write_float(std::basic_streambuf<StreamChar> &os, FloatType v, bool allow_inf = false, bool allow_nan = false) {
            static_assert(std::is_same<FloatType, float>::value ||
                          std::is_same<FloatType, double>::value ||
                          std::is_same<FloatType, long double>::value, "floating point type must be float, double, or long double");

            if ((!allow_inf && std::isinf(v)) ||
                (!allow_nan && std::isnan(v)))
                return false;

            // Ideally this would not use snprintf, but there's not really a great (fast) option in C++11

            char buf[512];
            auto chars = snprintf(buf, sizeof(buf),
                                  std::is_same<long double, FloatType>::value? "%.*Lg": "%.*g",
                                  std::numeric_limits<FloatType>::max_digits10 - 1, v);
            if (chars < 0)
                return false;
            else if (size_t(chars) < sizeof(buf))
                return os.sputn(buf, chars) == chars;

            std::string temp(chars + 1, '\0');
            chars = snprintf(&temp[0], temp.size(),
                             std::is_same<long double, FloatType>::value? "%.*Lg": "%.*g",
                             std::numeric_limits<FloatType>::max_digits10 - 1, v);
            if (chars >= 0 && size_t(chars) < temp.size())
                return os.sputn(temp.c_str(), chars) == chars;

            return false;
        }
    }

    // CSV
    template<typename Type>
    class CsvWriter;

    template<typename Type>
    CsvWriter<Type> csv(const Type &, unicode_codepoint separator = ',', unicode_codepoint quote = '"');

    template<typename Type>
    class CsvWriter {
        const Type &ref;
        const unicode_codepoint separator; // Supports Unicode characters as separator
        const unicode_codepoint quote; // Supports Unicode characters as quote

    public:
        constexpr CsvWriter(const Type &ref, unicode_codepoint separator = ',', unicode_codepoint quote = '"')
            : ref(ref)
            , separator(separator)
            , quote(quote) {}

        // Array overload, writes one line of CSV
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(begin(std::declval<_>()))>::value &&
                                                                                 !is_string_base<_>::value &&
                                                                                 !is_map_base<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            size_t index = 0;

            for (const auto &el: ref) {
                if (index)
                    put_unicode<StreamChar>{}(os, separator);

                os << csv(el, separator, quote);

                ++index;
            }

            os << '\n';
        }

        // String overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_base<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            // Underlying char type of string
            typedef typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type StringChar;

            const size_t sz = std::distance(begin(ref), end(ref));

            // Check for needing quotes first
            bool needs_quotes = false;

            for (size_t i = 0; i < sz; ) {
                const unicode_codepoint codepoint = get_unicode<StringChar>{}(ref, sz, i);

                if (codepoint == '\n' ||
                    codepoint == quote ||
                    codepoint == separator) {
                    needs_quotes = true;
                    break;
                }
            }

            if (needs_quotes)
                os << quote;

            // Then write out the actual string, escaping quotes
            for (size_t i = 0; i < sz; ) {
                const unicode_codepoint codepoint = get_unicode<StringChar>{}(ref, sz, i);

                if (codepoint == quote)
                    os << codepoint;

                os << codepoint;
            }

            if (needs_quotes)
                os << quote;
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &) const { }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            os << (ref? "true": "false");
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            os << std::dec << ref;
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            os << std::setprecision(std::numeric_limits<_>::max_digits10 - 1);

            // This CSV implementation doesn't support infinities or NAN, so just bomb out
            if (std::isinf(ref) || std::isnan(ref)) {
                os.setstate(std::ios_base::failbit);
            } else {
                os << ref;
            }
        }
    };

    template<typename Type>
    CsvWriter<Type> csv(const Type &value, unicode_codepoint separator, unicode_codepoint quote) { return CsvWriter<Type>(value, separator, quote); }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const CsvWriter<Type> &value) {
        value.write(os);
        return os;
    }

    template<typename Type>
    std::string to_csv(const Type &value) {
        std::ostringstream os;
        os << csv(value);
        return os? os.str(): std::string{};
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

#endif // SKATE_ADAPTERS_H
