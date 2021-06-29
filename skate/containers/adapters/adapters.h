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
    using std::begin;
    using std::end;

    // Helpers to get start and end of C-style string
    template<typename StrType, typename std::enable_if<std::is_pointer<StrType>::value, int>::type = 0>
    StrType begin(StrType c) { return c; }

    template<typename StrType, typename std::enable_if<std::is_pointer<StrType>::value, int>::type = 0>
    StrType end(StrType c) { while (*c) ++c; return c; }

    namespace impl {
        template<typename T> struct type_exists : public std::true_type { typedef int type; };

        // Determine if type is a string
        template<typename T> struct is_string_helper2 : public std::false_type {};
        template<typename... ContainerParams>
        struct is_string_helper2<std::basic_string<ContainerParams...>> : public std::true_type {};
        template<>
        struct is_string_helper2<char *> : public std::true_type {};
        template<>
        struct is_string_helper2<wchar_t *> : public std::true_type {};
        template<>
        struct is_string_helper2<char16_t *> : public std::true_type {};
        template<>
        struct is_string_helper2<char32_t *> : public std::true_type {};
#if __cplusplus >= 202002L
        template<>
        struct is_string_helper2<char8_t *> : public std::true_type {};
#endif
#if __cplusplus >= 201703L
        template<typename... ContainerParams>
        struct is_string_helper2<std::basic_string_view<ContainerParams...>> : public std::true_type {};
#endif

        // If base type is pointer, strip const/volatile off pointed-to type
        template<typename T>
        struct is_string_helper : public is_string_helper2<T> {};
        template<typename T>
        struct is_string_helper<T *> : public is_string_helper2<typename std::remove_cv<T>::type *> {};

        // Strip const/volatile off base type
        template<typename T>
        struct is_string : public is_string_helper<typename std::remove_cv<T>::type> {};

        // Determine if type is a map (iterators have first/second members)
        template<typename T>
        struct is_map {
            struct none {};

            template<typename U, typename _ = T> static std::pair<decltype(std::begin(std::declval<_>())->first), decltype(std::begin(std::declval<_>())->second)> f(U *);
            template<typename U> static none f(...);

            static constexpr int value = !std::is_same<none, decltype(f<T>(nullptr))>::value;
        };

        // Determine if type is an optional
        template<typename T> struct is_optional_helper : public std::false_type {};
        template<typename T> struct is_optional_helper<std::optional<T>> : public std::true_type {};

        template<typename T> struct is_optional : public is_optional_helper<typename std::remove_cv<T>::type> {};

        // Determine if type is a variant
        template<typename T> struct is_variant_helper : public std::false_type {};
        template<typename... Args> struct is_variant_helper<std::variant<Args...>> : public std::true_type {};

        template<typename T> struct is_variant : public is_variant_helper<typename std::remove_cv<T>::type> {};
    }

    template<typename Type>
    class JsonWriter;

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
        constexpr JsonWriter(const Type &ref, size_t indent = 0, size_t current_indentation = 0)
            : ref(ref)
            , current_indentation(current_indentation)
            , indent(indent)
        {}

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<impl::type_exists<decltype(begin(std::declval<_>()))>::value &&
                                                                                 !impl::is_string<_>::value &&
                                                                                 !impl::is_map<_>::value, int>::type = 0>
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
        template<typename StreamChar, typename _ = Type, typename std::enable_if<impl::is_map<_>::value &&
                                                                                 impl::is_string<decltype(begin(std::declval<_>())->first)>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            size_t index = 0;

            os << '{';

            for (const auto &el: ref) {
                if (index)
                    os << ',';

                if (indent)
                    do_indent(os, current_indentation + indent);

                os << json(el.first, indent, current_indentation + indent);
                os << ':';
                if (indent)
                    os << ' ';
                os << json(el.second, indent, current_indentation + indent);

                ++index;
            }

            if (indent && index) // Only indent if object is non-empty
                do_indent(os, current_indentation);

            os << '}';
        }

        // String overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<impl::is_string<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            // Underlying char type of string
            typedef typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type StringChar;

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
                        if (ch >= 0 && ch < 32) {
                            const char alphabet[] = "0123456789abcdef";

                            os << "\\u00";
                            os << alphabet[ch >> 4];
                            os << alphabet[ch & 0xf];
                            continue;
                        } else if (ch < 0x80) {
                            os << StreamChar(ch);
                        } else {
                            // Read Unicode in from string
                            size_t remaining = sz - i;
                            const unsigned long codepoint = get_unicode<StringChar>{}(&ref[i], remaining, nullptr);
                            i = sz - remaining - 1;

                            // Then add as a \u codepoint (or two, if surrogates are needed)
                            const char alphabet[] = "0123456789abcdef";
                            unsigned int hi, lo;

                            if (utf16surrogates(codepoint, hi, lo) == 0) {
                                os.setstate(os.rdstate() | std::ios_base::failbit);
                                return;
                            } else if (hi != lo) {
                                os << "\\u";
                                os << alphabet[hi >> 12];
                                os << alphabet[(hi >> 8) & 0xf];
                                os << alphabet[(hi >> 4) & 0xf];
                                os << alphabet[hi & 0xf];
                            }

                            os << "\\u";
                            os << alphabet[lo >> 12];
                            os << alphabet[(lo >> 8) & 0xf];
                            os << alphabet[(lo >> 4) & 0xf];
                            os << alphabet[lo & 0xf];
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
            os << std::dec << ref;
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

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<impl::is_optional<_>::value, int>::type = 0>
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
    JsonWriter<Type> json(const Type &value, size_t indent, size_t current_indentation) { return JsonWriter<Type>(value, indent, current_indentation); }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const JsonWriter<Type> &value) {
        value.write(os);
        return os;
    }

    template<typename Type>
    std::string to_json(const Type &value, size_t indent = 0, size_t current_indentation = 0) {
        std::ostringstream os;
        os << json(value, indent, current_indentation);
        return os.str();
    }

    // CSV
    template<typename Type>
    class CsvWriter;

    template<typename Type>
    CsvWriter<Type> csv(const Type &, int separator = ',', int quote = '"');

    template<typename Type>
    class CsvWriter {
        const Type &ref;
        const int separator; // Only supports ASCII characters as separator
        const int quote; // Only supports ASCII characters as quote

    public:
        constexpr CsvWriter(const Type &ref, int separator = ',', int quote = '"')
            : ref(ref)
            , separator(separator)
            , quote(quote) {}

        // Array overload, writes one line of CSV
        template<typename StreamChar, typename _ = Type, typename std::enable_if<impl::type_exists<decltype(begin(std::declval<_>()))>::value &&
                                                                                 !impl::is_string<_>::value &&
                                                                                 !impl::is_map<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            size_t index = 0;

            for (const auto &el: ref) {
                if (index)
                    os << StreamChar(separator);

                os << csv(el, separator, quote);

                ++index;
            }

            os << '\n';
        }

        // String overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<impl::is_string<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            // Underlying char type of string
            typedef typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type StringChar;

            const size_t sz = std::distance(begin(ref), end(ref));

            bool needs_quotes = false;
            for (const auto ch: ref) {
                if (ch == '\n' || ch == quote || ch == separator) {
                    needs_quotes = true;
                    break;
                }
            }

            if (needs_quotes)
                os << StreamChar(quote);

            for (size_t i = 0; i < sz; ++i) {
                const auto ch = ref[i];

                unsigned long codepoint = ch;

                // Does character size differ from stream type?
                // Conversion from wide string to narrow stream
                if (sizeof(StreamChar) < sizeof(StringChar)) {
                    if (utf16surrogate(codepoint) && sizeof(StringChar) == sizeof(char16_t))
                        codepoint = utf16codepoint(codepoint, ref[++i]);
                } else if (sizeof(StreamChar) > sizeof(StringChar)) {
                    // First get UTF-8 character from narrow string
                    size_t remaining = sz - i;
                    codepoint = utf8next_n((const char *) &ref[i], remaining, nullptr);
                    i = sz - remaining - 1;
                }

                // codepoint now contains current Unicode character in string

                // Then write the character. Loop runs once normally, twice if character is a quote
                for (size_t j = 0; j < 1 + (codepoint == quote); ++j) {
                    // Conversion from wide string to narrow stream
                    if (sizeof(StreamChar) < sizeof(StringChar)) {
                        char utf8[UTF8_MAX_CHAR_BYTES];
                        size_t remaining = sizeof(utf8);
                        if (utf8append(utf8, codepoint, remaining) == nullptr) {
                            os.setstate(os.rdstate() | std::ios_base::failbit);
                            return;
                        } else
                            os << utf8;
                    }
                    // Conversion from narrow string to wide stream
                    else if (sizeof(StreamChar) > sizeof(StringChar)) {
                        // First get UTF-8 character from narrow string
                        size_t remaining = sz - i;
                        const unsigned long codepoint = UTF_MASK & utf8next_n((const char *) &ref[i], remaining, nullptr);
                        i = sz - remaining - 1;

                        // Then convert codepoint to UTF-16/UTF-32
                        if (codepoint > UTF_MAX) {
                            os.setstate(os.rdstate() | std::ios_base::failbit);
                            return;
                        } else if (sizeof(StreamChar) == sizeof(char32_t)) // UTF-32
                            os << char32_t(codepoint);
                        else { // UTF-16
                            unsigned int hi, lo;

                            if (utf16surrogates(codepoint, hi, lo) == 0) {
                                os.setstate(os.rdstate() | std::ios_base::failbit);
                                return;
                            } else if (hi != lo)
                                os << char16_t(hi);

                            os << char16_t(lo);
                        }
                    }
                    // Default case is just to write the char as it's the same size as the stream char
                    //
                    // Note that this can fail if a wide character is written to a wide stream with no locale setting
                    else {
                        os << ch;
                    }
                }
            }

            if (needs_quotes)
                os << StreamChar(quote);
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

            // CSV doesn't support infinities or NAN, so just bomb out
            if (std::isinf(ref) || std::isnan(ref)) {
                os.setstate(os.rdstate() | std::ios_base::failbit);
            } else {
                os << ref;
            }
        }
    };

    template<typename Type>
    CsvWriter<Type> csv(const Type &value, int separator, int quote) { return CsvWriter<Type>(value, separator, quote); }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const CsvWriter<Type> &value) {
        value.write(os);
        return os;
    }

    template<typename Type>
    std::string to_csv(const Type &value) {
        std::ostringstream os;
        os << csv(value);
        return os.str();
    }

    // XML
    namespace impl {
        template<typename CharType>
        inline bool xml_is_name_start_char(CharType c) {
            const typename std::make_unsigned<CharType>::type ch = c;

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

        template<typename CharType>
        inline bool xml_is_name_char(CharType c) {
            const typename std::make_unsigned<CharType>::type ch = c;

            return (ch == '-' || ch == '.') ||
                   (ch >= '0' && ch <= '9') ||
                   (ch == 0xb7) ||
                   (ch >= 0x300 && ch <= 0x36f) ||
                   (ch == 0x203f || ch == 0x2040) ||
                   xml_is_name_start_char(c);
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
        template<typename StreamChar, typename _ = Type, typename std::enable_if<impl::type_exists<decltype(begin(std::declval<_>()))>::value &&
                                                                                 !impl::is_string<_>::value &&
                                                                                 !impl::is_map<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            for (const auto &el: ref) {
                os << xml(el, indent, current_indentation);
            }
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<impl::is_map<_>::value &&
                                                                                 impl::is_string<decltype(begin(std::declval<_>())->first)>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os) const {
            size_t index = 0;

            for (const auto &el: ref) {
                if (indent && index)
                    do_indent(os, current_indentation);

                // Write start tag
                os << '<';
                xml(el.first, indent, current_indentation + indent).write(os, true);
                os << '>';

                // Write body, special case if indenting since empty tags should stay on the same line
                if (indent) {
                    std::basic_string<StreamChar> str;

                    {
                        std::basic_ostringstream<StreamChar> s;

                        // Write body to temporary stream
                        s << xml(el.second, indent, current_indentation + indent);

                        str = s.str();
                    }

                    if (str.size()) {
                        do_indent(os, current_indentation + indent);
                        os << str;
                        do_indent(os, current_indentation);
                    }
                } else {
                    os << xml(el.second, indent, current_indentation + indent);
                }

                // Write end tag
                os << "</";
                xml(el.first, indent, current_indentation + indent).write(os, true);
                os << '>';

                ++index;
            }
        }

        // String overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<impl::is_string<_>::value, int>::type = 0>
        void write(std::basic_ostream<StreamChar> &os, bool is_tag = false) const {
            // Underlying char type of string
            typedef typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type StringChar;

            const size_t sz = std::distance(begin(ref), end(ref));

            for (size_t i = 0; i < sz; ++i) {
                const auto ch = ref[i];

                unsigned long codepoint;

                if (sizeof(StreamChar) == sizeof(char)) { // UTF-8

                }

                switch (ch) {
                    case '&': os << "&amp;"; break;
                    case '"': os << "&quot;"; break;
                    case '\'': os << "&apos;"; break;
                    case '<': os << "&lt;"; break;
                    case '>': os << "&gt;"; break;
                    default: {
                        // Does character size differ from stream type?
                        // Conversion from wide string to narrow stream
                        if (sizeof(StreamChar) < sizeof(StringChar)) {
                            // First get UTF-16/UTF-32 codepoint from wide string
                            unsigned long codepoint = ch;

                            if (utf16surrogate(codepoint) && sizeof(StringChar) == sizeof(char16_t))
                                codepoint = utf16codepoint(codepoint, ref[++i]);

                            // Check if it's a valid tag character
                            if (is_tag) {
                                const bool valid_tag_char = i == 0? impl::xml_is_name_start_char(codepoint): impl::xml_is_name_char(codepoint);

                                if (!valid_tag_char) {
                                    os.setstate(os.rdstate() | std::ios_base::failbit);
                                    return;
                                }
                            }

                            // Then add as UTF-8
                            char utf8[UTF8_MAX_CHAR_BYTES];
                            size_t remaining = sizeof(utf8);
                            if (utf8append(utf8, codepoint, remaining) == nullptr) {
                                os.setstate(os.rdstate() | std::ios_base::failbit);
                                return;
                            } else
                                os << utf8;
                        }
                        // Conversion from narrow string to wide stream
                        else if (sizeof(StreamChar) > sizeof(StringChar)) {
                            // First get UTF-8 character from narrow string
                            size_t remaining = sz - i;
                            const unsigned long codepoint = utf8next_n((const char *) &ref[i], remaining, nullptr);
                            i = sz - remaining - 1;

                            // Check if it's a valid tag character
                            if (is_tag) {
                                const bool valid_tag_char = i == 0? impl::xml_is_name_start_char(codepoint): impl::xml_is_name_char(codepoint);

                                if (!valid_tag_char) {
                                    os.setstate(os.rdstate() | std::ios_base::failbit);
                                    return;
                                }
                            }

                            // Then convert codepoint to UTF-16/UTF-32
                            if (codepoint > UTF_MAX) {
                                os.setstate(os.rdstate() | std::ios_base::failbit);
                                return;
                            } else if (sizeof(StreamChar) == sizeof(char32_t)) // UTF-32
                                os << char32_t(codepoint);
                            else { // UTF-16
                                unsigned int hi, lo;

                                if (utf16surrogates(codepoint, hi, lo) == 0) {
                                    os.setstate(os.rdstate() | std::ios_base::failbit);
                                    return;
                                } else if (hi != lo)
                                    os << char16_t(hi);

                                os << char16_t(lo);
                            }
                        }
                        // Default case is just to write the char as it's the same size as the stream char
                        //
                        // Note that this can fail if a wide character is written to a wide stream with no locale setting
                        else {
                            os << ch;
                        }
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
        return os.str();
    }

    template<typename Type>
    std::string to_xml_doc(const Type &value) {
        std::ostringstream os;
        os << xml_doc(value);
        return os.str();
    }
}

#endif // ADAPTERS_H
