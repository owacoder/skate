#ifndef ADAPTERS_H
#define ADAPTERS_H

#include <cmath>
#include <type_traits>
#include <limits>
#include <iostream>
#include <iomanip>
#include <sstream>

#if __cplusplus >= 201703L
# include <optional>
# include <variant>
#endif

#include "utf.h"

namespace Skate {
    // Helpers to get start and end of C-style string
    using std::begin;
    using std::end;

    template<typename StrType, typename std::enable_if<std::is_pointer<StrType>::value, int>::type = 0>
    StrType begin(StrType c) { return c; }

    template<typename StrType, typename std::enable_if<std::is_pointer<StrType>::value, int>::type = 0>
    StrType end(StrType c) { while (*c) ++c; return c; }

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
    struct map_pair_helper {
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
    struct map_helper {
        struct none {};

        template<typename U, typename _ = Map> static typename std::enable_if<map_pair_helper<decltype(begin(std::declval<_>()))>::value, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Map>(nullptr))>::value;
    };

    template<typename Map>
    struct string_map_helper {
        struct none {};

        template<typename U, typename _ = Map> static typename std::enable_if<is_string_base<typename map_pair_helper<decltype(begin(std::declval<_>()))>::key_type>::value, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Map>(nullptr))>::value;
    };

    template<typename MapPair, typename = typename map_pair_helper<MapPair>::key_type> struct key_of;

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

    template<typename MapPair, typename = typename map_pair_helper<MapPair>::value_type> struct value_of;

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
                                                             map_helper<typename base_type<T>::type>::value> {};

    template<typename T>
    struct is_string_map_base : public std::integral_constant<bool, is_map_base<T>::value &&
                                                                    string_map_helper<T>::value> {};

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

            if ((is >> c) && c != expected)
                is.setstate(is.rdstate() | std::ios_base::failbit);

            return is;
        }
    }

    template<typename Type>
    class JsonReader;

    template<typename Type>
    class JsonWriter;

    template<typename Type>
    JsonReader<Type> json(Type &);

    template<typename Type>
    class JsonReader {
        Type &ref;

        template<typename> friend class JsonWriter;

    public:
        constexpr JsonReader(Type &ref) : ref(ref) {}

        // User object overload, skate_to_json(stream, object)
        template<typename StreamChar, typename _ = Type, typename type_exists<decltype(skate_json(std::declval<std::basic_istream<StreamChar> &>(), std::declval<_>()))>::type = 0>
        void write(std::basic_istream<StreamChar> &is) const {
            // Library user is responsible for validating read JSON in the callback function
            skate_json(is, ref);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(begin(std::declval<_>()))>::value &&
                                                                                 !is_string_base<_>::value &&
                                                                                 !is_map_base<_>::value, int>::type = 0>
        void read(std::basic_istream<StreamChar> &is) {
            typedef typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type Element;

            StreamChar c;

            ref.clear();

            // Read start char
            is >> std::skipws >> c;
            if (c != '[')
                goto error;

            // Empty array?
            is >> c;
            if (c == ']')
                return;
            is.putback(c);

            do {
                Element element;

                is >> json(element);
                ref.push_back(std::move(element));

                is >> std::skipws >> c;
                if (c == ']')
                    return;
            } while (c == ',');

            // Invalid if here, as function should have returned inside loop with valid array
        error:
            ref.clear();
            is.setstate(is.rdstate() | std::ios_base::failbit);
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        void read(std::basic_istream<StreamChar> &is) {
            typedef typename std::remove_cv<typename std::remove_reference<decltype(key_of<decltype(begin(std::declval<_>()))>{}(begin(std::declval<_>())))>::type>::type Key;
            typedef typename std::remove_cv<typename std::remove_reference<decltype(value_of<decltype(begin(std::declval<_>()))>{}(begin(std::declval<_>())))>::type>::type Value;

            StreamChar c;

            ref.clear();

            // Read start char
            is >> std::skipws >> c;
            if (c != '{')
                goto error;

            // Empty object?
            is >> c;
            if (c == '}')
                return;
            is.putback(c);

            do {
                Key key;
                Value value;

                // Guarantee a string as key to be parsed
                is >> std::skipws >> c;
                if (c != '"')
                    goto error;
                is.putback(c);

                is >> json(key);

                is >> std::skipws >> c;
                if (c != ':')
                    goto error;

                is >> json(value);

                ref[std::move(key)] = std::move(value);

                is >> std::skipws >> c;
                if (c == '}')
                    return;
            } while (c == ',');

            // Invalid if here, as function should have returned inside loop with valid array
        error:
            ref.clear();
            is.setstate(is.rdstate() | std::ios_base::failbit);
        }

        // String overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_base<_>::value, int>::type = 0>
        void read(std::basic_istream<StreamChar> &is) {
            // Underlying char type of string
            typedef typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type StringChar;

            unicode_codepoint codepoint;
            StreamChar c;

            ref.clear();

            // Read start char
            is >> std::skipws >> c;
            if (c != '"')
                goto error;

            is >> std::noskipws;

            while (is >> codepoint) {
                // End of string?
                if (codepoint == '"') {
                    return;
                }
                // Escaped character sequence?
                else if (codepoint == '\\') {
                    if (!(is >> c))
                        goto error;

                    switch (c) {
                        case '"':
                        case '\\':
                        case 'b':
                        case 'f':
                        case 'n':
                        case 'r':
                        case 't': codepoint = c; break;
                        case 'u': {
                            char digits[5] = { 0 };
                            unsigned int hi = 0;

                            if (!is.get(digits, 5))
                                goto error;

                            for (size_t i = 0; i < 4; ++i) {
                                const int digit = impl::toxdigit(digits[i]);
                                if (digit < 0)
                                    goto error;

                                hi = (hi << 4) | digit;
                            }

                            if (utf16surrogate(hi)) {
                                unsigned int lo = 0;

                                if ((is.get() != '\\') ||
                                    (is.get() != 'u') ||
                                    !is.get(digits, 5))
                                    goto error;

                                for (size_t i = 0; i < 4; ++i) {
                                    const int digit = impl::toxdigit(digits[i]);
                                    if (digit < 0)
                                        goto error;

                                    lo = (lo << 4) | digit;
                                }

                                codepoint = utf16codepoint(hi, lo);
                            } else
                                codepoint = hi;

                            break;
                        }
                        default: goto error;
                    }
                }

                if (!put_unicode<StringChar>{}(ref, codepoint))
                    goto error;
            }

        error:
            ref.clear();
            is.setstate(is.rdstate() | std::ios_base::failbit);
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        void read(std::basic_istream<StreamChar> &is) const {
            is >> std::skipws;
            impl::expect_char(is, 'n');
            is >> std::noskipws;
            impl::expect_char(is, 'u');
            impl::expect_char(is, 'l');
            impl::expect_char(is, 'l');
        }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        void read(std::basic_istream<StreamChar> &is) const {
            StreamChar c;

            ref = false;
            if (is >> std::skipws >> c) {
                switch (c) {
                    case 't':
                        is >> std::noskipws;
                        impl::expect_char(is, 'r');
                        impl::expect_char(is, 'u');
                        impl::expect_char(is, 'e');
                        break;
                    case 'f':
                        is >> std::noskipws;
                        impl::expect_char(is, 'a');
                        impl::expect_char(is, 'l');
                        impl::expect_char(is, 's');
                        impl::expect_char(is, 'e');
                        break;
                    default:
                        is.setstate(is.rdstate() | std::ios_base::failbit);
                        break;
                }
            }
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        void read(std::basic_istream<StreamChar> &is) const {
            // Allows writing chars to be cast up to int and preventing from being written as an individual char
            typedef typename std::conditional<sizeof(_) < sizeof(int), int,
                    typename std::conditional<!std::is_same<_, StreamChar>::value, _,
                    typename std::conditional<std::is_signed<_>::value, std::intmax_t, std::uintmax_t>::type>::type>::type Cast;

            Cast temp = 0;

            is >> std::skipws >> std::dec >> temp;

            if (is && (temp > std::numeric_limits<_>::max() || temp < std::numeric_limits<_>::min())) {
                is.setstate(is.rdstate() | std::ios_base::failbit);
                ref = 0;
            } else {
                ref = temp;
            }
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        void read(std::basic_istream<StreamChar> &is) const {
            is >> ref;
        }

        // Smart pointer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_shared_ptr_base<_>::value ||
                                                                                 is_unique_ptr_base<_>::value, int>::type = 0>
        void read(std::basic_istream<StreamChar> &is) const {
            StreamChar c;

            ref.reset();
            if (is >> std::skipws >> c) {
                if (c == 'n') {
                    is >> std::noskipws;
                    impl::expect_char(is, 'u');
                    impl::expect_char(is, 'l');
                    impl::expect_char(is, 'l');
                } else {
                    is.putback(c);

                    ref.reset(new Type{});

                    is >> json(*ref);
                }
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_optional_base<_>::value, int>::type = 0>
        void read(std::basic_istream<StreamChar> &is) const {
            StreamChar c;

            ref.reset();
            if (is >> std::skipws >> c) {
                if (c == 'n') {
                    is >> std::noskipws;
                    impl::expect_char(is, 'u');
                    impl::expect_char(is, 'l');
                    impl::expect_char(is, 'l');
                } else {
                    is.putback(c);

                    ref = Type{};

                    is >> json(*ref);
                }
            }
        }
#endif
    };

    template<typename Type>
    JsonWriter<Type> json(const Type &, size_t indent = 0, size_t current_indentation = 0);

    template<typename Type>
    class JsonWriter {
        const Type &ref;
        const size_t current_indentation; // Current indentation depth in number of spaces
        const size_t indent; // Indent per level in number of spaces

        template<typename StreamChar>
        void do_indent(std::basic_ostream<StreamChar> &os, size_t sz) const {
            os << '\n';
            for (size_t i = 0; i < sz; ++i)
                os << ' ';
        }

    public:
        constexpr JsonWriter(const JsonReader<Type> &reader, size_t indent = 0, size_t current_indentation = 0)
            : ref(reader.ref)
            , current_indentation(current_indentation)
            , indent(indent)
        {}
        constexpr JsonWriter(const Type &ref, size_t indent = 0, size_t current_indentation = 0)
            : ref(ref)
            , current_indentation(current_indentation)
            , indent(indent)
        {}

        // User object overload, skate_to_json(stream, object)
        template<typename StreamChar, typename _ = Type, typename type_exists<decltype(skate_json(static_cast<std::basic_ostream<StreamChar> &>(std::declval<std::basic_ostream<StreamChar> &>()), std::declval<_>()))>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            // Library user is responsible for creating valid JSON in the callback function
            skate_json(os, ref);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(begin(std::declval<_>()))>::value &&
                                                                                 !is_string_base<_>::value &&
                                                                                 !is_map_base<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            size_t index = 0;

            os << '[';

            for (const auto &el: ref) {
                if (index)
                    os << ',';

                if (indent)
                    do_indent(os, current_indentation + indent);

                os << json(el, indent, current_indentation + indent);

                ++index;
            }

            if (indent && index) // Only indent if array is non-empty
                do_indent(os, current_indentation);

            os << ']';
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            typedef typename base_type<decltype(begin(ref))>::type KeyValuePair;

            size_t index = 0;

            os << '{';

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (index)
                    os << ',';

                if (indent)
                    do_indent(os, current_indentation + indent);

                os << json(key_of<KeyValuePair>{}(el), indent, current_indentation + indent);
                os << ':';
                if (indent)
                    os << ' ';
                os << json(value_of<KeyValuePair>{}(el), indent, current_indentation + indent);

                ++index;
            }

            if (indent && index) // Only indent if object is non-empty
                do_indent(os, current_indentation);

            os << '}';
        }

        // String overload
        template<typename StreamChar,
                 typename _ = Type,
                 typename StringChar = typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type,
                 typename std::enable_if<type_exists<decltype(unicode_codepoint(std::declval<StringChar>()))>::value &&
                                         is_string_base<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            const size_t sz = std::distance(begin(ref), end(ref));

            os << '"';

            for (size_t i = 0; i < sz; ++i) {
                const auto ch = ref[i];

                switch (ch) {
                    case '"': os << "\\\""; break;
                    case '\\': os << "\\\\"; break;
                    case '\b': os << "\\b"; break;
                    case '\f': os << "\\f"; break;
                    case '\n': os << "\\n"; break;
                    case '\r': os << "\\r"; break;
                    case '\t': os << "\\t"; break;
                    default: {
                        // Is character a control character? If so, write it out as a Unicode constant
                        // All other ASCII characters just get passed through
                        if (ch >= 32 && ch < 0x80) {
                            os.put(ch);
                        } else {
                            // Read Unicode in from string
                            const unicode_codepoint codepoint = get_unicode<StringChar>{}(ref, sz, i);

                            // Then add as a \u codepoint (or two, if surrogates are needed)
                            const char alphabet[] = "0123456789abcdef";
                            unsigned int hi, lo;

                            switch (utf16surrogates(codepoint.value(), hi, lo)) {
                                case 2:
                                    os << "\\u";
                                    os << alphabet[hi >> 12];
                                    os << alphabet[(hi >> 8) & 0xf];
                                    os << alphabet[(hi >> 4) & 0xf];
                                    os << alphabet[hi & 0xf];
                                    // fallthrough
                                case 1:
                                    os << "\\u";
                                    os << alphabet[lo >> 12];
                                    os << alphabet[(lo >> 8) & 0xf];
                                    os << alphabet[(lo >> 4) & 0xf];
                                    os << alphabet[lo & 0xf];
                                    break;
                                default:
                                    os.setstate(os.rdstate() | std::ios_base::failbit);
                                    return;
                            }
                        }

                        break;
                    }
                }
            }

            os << '"';
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            os << "null";
        }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            os << (ref? "true": "false");
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            // Allows writing chars to be cast up to int and preventing from being written as an individual char
            typedef typename std::conditional<sizeof(_) < sizeof(int), int,
                    typename std::conditional<!std::is_same<_, StreamChar>::value, _,
                    typename std::conditional<std::is_signed<_>::value, std::intmax_t, std::uintmax_t>::type>::type>::type Cast;

            os << std::dec << Cast(ref);
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            os << std::setprecision(std::numeric_limits<_>::max_digits10 - 1);

            // JSON doesn't support infinities or NAN, so just bomb out
            if (std::isinf(ref) || std::isnan(ref)) {
                os.setstate(os.rdstate() | std::ios_base::failbit);
            } else {
                os << ref;
            }
        }

        // Smart pointer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_shared_ptr_base<_>::value ||
                                                                                 is_weak_ptr_base<_>::value ||
                                                                                 is_unique_ptr_base<_>::value ||
                                                                                 std::is_pointer<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            if (!ref) {
                os << "null";
            } else {
                os << json(*ref, indent, current_indentation);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_optional_base<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            if (!ref) {
                os << "null";
            } else {
                os << json(*ref, indent, current_indentation);
            }
        }
#endif
    };

    template<typename Type>
    JsonReader<Type> json(Type &value) { return JsonReader<Type>(value); }

    template<typename Type>
    JsonWriter<Type> json(const Type &value, size_t indent, size_t current_indentation) { return JsonWriter<Type>(value, indent, current_indentation); }

    template<typename StreamChar, typename Type>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, JsonReader<Type> value) {
        value.read(is);
        return is;
    }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, JsonReader<Type> value) {
        return os << JsonWriter<Type>(value);
    }
    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, JsonWriter<Type> value) {
        value.write(os);
        return os;
    }

    template<typename Type>
    Type from_json(const std::string &s) {
        Type value;
        std::istringstream is{s};
        is >> json(value);
        return value;
    }

    template<typename Type>
    std::string to_json(const Type &value, size_t indent = 0, size_t current_indentation = 0) {
        std::ostringstream os;
        os << json(value, indent, current_indentation);
        return os? os.str(): std::string{};
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
                os.setstate(os.rdstate() | std::ios_base::failbit);
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

    // XML
    namespace impl {
        inline bool xml_is_name_start_char(unicode_codepoint ch) {
            return (ch >= 'A' && ch <= 'Z') ||
                   (ch >= 'a' && ch <= 'z') ||
                   (ch == ':' || ch == '_') ||
                   (ch >= 0xc0 && ch <= 0x2ff && ch != 0xd7 && ch != 0xf7) ||
                   (ch >= 0x370 && ch <= 0x1fff && ch != 0x37e) ||
                   (ch == 0x200c || ch == 0x200d) ||
                   (ch >= 0x2070 && ch <= 0x218f) ||
                   (ch >= 0x2c00 && ch <= 0x2fef) ||
                   (ch >= 0x3001 && ch <= 0xd7ff) ||
                   (ch >= 0xf900 && ch <= 0xfdcf) ||
                   (ch >= 0xfdf0 && ch <= 0xeffff && ch != 0xffff);
        }

        inline bool xml_is_name_char(unicode_codepoint ch) {
            return (ch == '-' || ch == '.') ||
                   (ch >= '0' && ch <= '9') ||
                   (ch == 0xb7) ||
                   (ch >= 0x300 && ch <= 0x36f) ||
                   (ch == 0x203f || ch == 0x2040) ||
                   xml_is_name_start_char(ch);
        }
    }

    template<typename Type>
    class XmlWriter;

    template<typename Type>
    XmlWriter<Type> xml(const Type &, size_t indent = 0, size_t current_indentation = 0);

    template<typename Type>
    class XmlWriter {
        const Type &ref;
        const size_t current_indentation; // Current indentation depth in number of spaces
        const size_t indent; // Indent per level in number of spaces

        template<typename Stream>
        void do_indent(Stream &os, size_t sz) const {
            os << '\n';
            for (size_t i = 0; i < sz; ++i)
                os << ' ';
        }

    public:
        constexpr XmlWriter(const Type &ref, size_t indent = 0, size_t current_indentation = 0)
            : ref(ref)
            , current_indentation(current_indentation)
            , indent(indent)
        {}

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(begin(std::declval<_>()))>::value &&
                                                                                 !is_string_base<_>::value &&
                                                                                 !is_map_base<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            for (const auto &el: ref) {
                os << xml(el, indent, current_indentation);
            }
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            typedef typename base_type<decltype(begin(ref))>::type KeyValuePair;

            size_t index = 0;

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (indent && index)
                    do_indent(os, current_indentation);

                // Write start tag
                os << '<';
                xml(key_of<KeyValuePair>{}(el), indent, current_indentation + indent).write(os, true);
                os << '>';

                // Write body, special case if indenting since empty tags should stay on the same line
                if (indent) {
                    std::basic_string<StreamChar> str;

                    {
                        std::basic_ostringstream<StreamChar> s;

                        // Write body to temporary stream
                        s << xml(value_of<KeyValuePair>{}(el), indent, current_indentation + indent);

                        str = s.str();
                    }

                    if (str.size()) {
                        do_indent(os, current_indentation + indent);
                        os << str;
                        do_indent(os, current_indentation);
                    }
                } else {
                    os << xml(value_of<KeyValuePair>{}(el), indent, current_indentation + indent);
                }

                // Write end tag
                os << "</";
                xml(key_of<KeyValuePair>{}(el), indent, current_indentation + indent).write(os, true);
                os << '>';

                ++index;
            }
        }

        // String overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_base<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os, bool is_tag = false) const {
            // Underlying char type of string
            typedef typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type StringChar;

            const size_t sz = std::distance(begin(ref), end(ref));

            for (size_t i = 0; i < sz; ) {
                const unicode_codepoint codepoint = get_unicode<StringChar>{}(ref, sz, i);

                // Check if it's a valid tag character
                if (is_tag) {
                    const bool valid_tag_char = i == 0? impl::xml_is_name_start_char(codepoint): impl::xml_is_name_char(codepoint);

                    if (!valid_tag_char) {
                        os.setstate(os.rdstate() | std::ios_base::failbit);
                        return;
                    }

                    os << codepoint;
                } else { // Content character
                    switch (codepoint.value()) {
                        case '&': os << "&amp;"; break;
                        case '"': os << "&quot;"; break;
                        case '\'': os << "&apos;"; break;
                        case '<': os << "&lt;"; break;
                        case '>': os << "&gt;"; break;
                        default:
                            os << codepoint;
                            break;
                    }
                }
            }
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

            // XML doesn't support infinities or NAN, so just bomb out
            if (std::isinf(ref) || std::isnan(ref)) {
                os.setstate(os.rdstate() | std::ios_base::failbit);
            } else {
                os << ref;
            }
        }
    };

    template<typename Type>
    class XmlDocWriter {
        const Type &ref;
        const size_t current_indentation; // Current indentation depth in number of spaces
        const size_t indent; // Indent per level in number of spaces

    public:
        constexpr XmlDocWriter(const Type &ref, size_t indent = 0, size_t current_indentation = 0)
            : ref(ref)
            , current_indentation(current_indentation)
            , indent(indent)
        {}

        template<typename StreamChar>
        void write(std::basic_ostream<StreamChar> &os) const {
            os << "<?xml version=\"1.0\"?>\n" << xml(ref, indent, current_indentation);
        }
    };

    template<typename Type>
    XmlWriter<Type> xml(const Type &value, size_t indent, size_t current_indentation) { return XmlWriter<Type>(value, indent, current_indentation); }

    template<typename Type>
    XmlDocWriter<Type> xml_doc(const Type &value, size_t indent = 0, size_t current_indentation = 0) { return XmlDocWriter<Type>(value, indent, current_indentation); }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const XmlWriter<Type> &value) {
        value.write(os);
        return os;
    }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const XmlDocWriter<Type> &value) {
        value.write(os);
        return os;
    }

    template<typename Type>
    std::string to_xml(const Type &value) {
        std::ostringstream os;
        os << xml(value);
        return os? os.str(): std::string{};
    }

    template<typename Type>
    std::string to_xml_doc(const Type &value) {
        std::ostringstream os;
        os << xml_doc(value);
        return os? os.str(): std::string{};
    }
}

// Qt helpers
#ifdef QT_VERSION
    namespace Skate {
        template<>
        struct is_string<QByteArray> : public std::true_type {};

        template<>
        struct is_string<QString> : public std::true_type {};
    }

    template<typename StreamChar>
    std::basic_istream<StreamChar> &skate_json(std::basic_istream<StreamChar> &is, const QString &str) {
        std::wstring wstr;
        is >> Skate::json(wstr);
        str = wstr;
        return is;
    }

    template<typename StreamChar>
    std::basic_ostream<StreamChar> &skate_json(std::basic_ostream<StreamChar> &os, const QString &str) {
        return os << Skate::json(str.toStdWString());
    }
#endif

#endif // ADAPTERS_H
