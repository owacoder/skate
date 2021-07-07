#ifndef SKATE_JSON_H
#define SKATE_JSON_H

#include "core.h"

namespace skate {
    template<typename Type>
    class json_reader;

    template<typename Type>
    class json_writer;

    template<typename Type>
    json_reader<Type> json(Type &);

    template<typename Type>
    class json_reader {
        Type &ref;

        template<typename> friend class json_writer;

    public:
        constexpr json_reader(Type &ref) : ref(ref) {}

        // User object overload, skate_to_json(stream, object)
        template<typename StreamChar, typename _ = Type, typename type_exists<decltype(skate_json(std::declval<std::basic_streambuf<StreamChar> &>(), std::declval<_ &>()))>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            // Library user is responsible for validating read JSON in the callback function
            return skate_json(is, ref);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(begin(std::declval<_>()))>::value &&
                                                                                 !is_string_base<_>::value &&
                                                                                 !is_map_base<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type Element;

            ref.clear();

            // Read start char
            if (!impl::skipws(is) || is.sbumpc() != '[')
                return false;

            if (!impl::skipws(is))
                return false;
            else if (is.sgetc() == ']') { // Empty array?
                return is.sbumpc() != std::char_traits<StreamChar>::eof();
            }

            do {
                Element element;

                if (!json(element).read(is))
                    return false;

                ref.push_back(std::move(element));

                if (!impl::skipws(is))
                    return false;
                else if (is.sgetc() == ']')
                    return is.sbumpc() == ']';
            } while (is.sbumpc() == ',');

            // Invalid if here, as function should have returned inside loop with valid array
            ref.clear();
            return false;
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::remove_cv<typename std::remove_reference<decltype(key_of<decltype(begin(std::declval<_>()))>{}(begin(std::declval<_>())))>::type>::type Key;
            typedef typename std::remove_cv<typename std::remove_reference<decltype(value_of<decltype(begin(std::declval<_>()))>{}(begin(std::declval<_>())))>::type>::type Value;

            ref.clear();

            // Read start char
            if (!impl::skipws(is) || is.sbumpc() != '{')
                return false;

            if (!impl::skipws(is))
                return false;
            else if (is.sgetc() == '}') { // Empty object?
                return is.sbumpc() != std::char_traits<StreamChar>::eof();
            }

            do {
                Key key;
                Value value;

                // Guarantee a string as key to be parsed
                if (!impl::skipws(is) || is.sgetc() != '"')
                    return false;

                if (!json(key).read(is))
                    return false;

                if (!impl::skipws(is) || is.sbumpc() != ':')
                    return false;

                if (!json(value).read(is))
                    return false;

                ref[std::move(key)] = std::move(value);

                if (!impl::skipws(is))
                    return false;
                else if (is.sgetc() == '}')
                    return is.sbumpc() == '}';
            } while (is.sbumpc() == ',');

            // Invalid if here, as function should have returned inside loop with valid array
            ref.clear();
            return false;
        }

        // String overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_base<_>::value &&
                                                                                 type_exists<decltype(
                                                                                             // Only if put_unicode is available
                                                                                             std::declval<put_unicode<typename base_type<decltype(*begin(std::declval<_>()))>::type>>()(std::declval<_ &>(), std::declval<unicode_codepoint>())
                                                                                             )>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            // Underlying char type of string
            typedef typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type StringChar;

            unicode_codepoint codepoint;

            ref.clear();

            // Read start char
            if (!impl::skipws(is) || is.sbumpc() != '"')
                return false;

            while (get_unicode<StreamChar>{}(is, codepoint)) {
                // End of string?
                if (codepoint == '"') {
                    return true;
                }
                // Escaped character sequence?
                else if (codepoint == '\\') {
                    const auto c = is.sbumpc();

                    switch (c) {
                        case std::char_traits<StreamChar>::eof(): return false;
                        case '"':
                        case '\\':
                        case 'b':
                        case 'f':
                        case 'n':
                        case 'r':
                        case 't': codepoint = c; break;
                        case 'u': {
                            StreamChar digits[4] = { 0 };
                            unsigned int hi = 0;

                            if (is.sgetn(digits, 4) != 4)
                                return false;

                            for (size_t i = 0; i < 4; ++i) {
                                const int digit = impl::toxdigit(digits[i]);
                                if (digit < 0)
                                    return false;

                                hi = (hi << 4) | digit;
                            }

                            if (utf16surrogate(hi)) {
                                unsigned int lo = 0;

                                if ((is.sbumpc() != '\\') ||
                                    (is.sbumpc() != 'u') ||
                                    is.sgetn(digits, 4) != 4)
                                    return false;

                                for (size_t i = 0; i < 4; ++i) {
                                    const int digit = impl::toxdigit(digits[i]);
                                    if (digit < 0)
                                        return false;

                                    lo = (lo << 4) | digit;
                                }

                                codepoint = utf16codepoint(hi, lo);
                            } else
                                codepoint = hi;

                            break;
                        }
                        default: return false;
                    }
                }

                if (!put_unicode<StringChar>{}(ref, codepoint))
                    return false;
            }

            ref.clear();
            return false;
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            if (!impl::skipws(is))
                return false;

            return is.sbumpc() == 'n' &&
                    is.sbumpc() == 'u' &&
                    is.sbumpc() == 'l' &&
                    is.sbumpc() == 'l';
        }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref = false;
            if (!impl::skipws(is))
                return false;

            switch (is.sbumpc()) {
                case 't':
                    if (is.sbumpc() == 'r' &&
                        is.sbumpc() == 'u' &&
                        is.sbumpc() == 'e') {
                        return ref = true;
                    }
                    return false;
                case 'f':
                    return is.sbumpc() == 'a' &&
                            is.sbumpc() == 'l' &&
                            is.sbumpc() == 's' &&
                            is.sbumpc() == 'e';
                    break;
                default: return false;
            }
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            std::string temp;

            ref = 0;
            if (!impl::skipws(is))
                return false;

            auto c = is.sgetc();
            if (!impl::isdigit(c) && c != '-')
                return false;

            temp.push_back(char(c));

            while (true) {
                const auto c = is.snextc();

                if (c == std::char_traits<StreamChar>::eof() || !impl::isdigit(c))
                    break;

                temp.push_back(char(c));
            }

            char *end = nullptr;
            if (temp[0] == '-') {
                errno = 0;
                const auto v = strtoll(temp.c_str(), &end, 10);
                if (errno == ERANGE || v > std::numeric_limits<_>::max() || v < std::numeric_limits<_>::min())
                    return false;

                if (std::is_unsigned<_>::value && v < 0)
                    return false;

                ref = Type(v);
            } else {
                errno = 0;
                const auto v = strtoull(temp.c_str(), &end, 10);
                if (errno == ERANGE || v > std::numeric_limits<_>::max())
                    return false;

                ref = Type(v);
            }

            if (*end != 0) {
                ref = 0;
                return false;
            }

            return true;
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref = 0;
            if (!impl::skipws(is))
                return false;

            return impl::read_float(is, ref);
        }

        // Smart pointer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_shared_ptr_base<_>::value ||
                                                                                 is_unique_ptr_base<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref.reset();
            if (!impl::skipws(is))
                return false;

            if (is.sgetc() == 'n') {
                return is.sbumpc() == 'n' &&
                        is.sbumpc() == 'u' &&
                        is.sbumpc() == 'l' &&
                        is.sbumpc() == 'l';
            } else {
                ref.reset(new Type{});

                return json(*ref).read(is);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_optional_base<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref.reset();
            if (!impl::skipws(is))
                return false;

            if (is.sgetc() == 'n') {
                return is.sbumpc() == 'n' &&
                        is.sbumpc() == 'u' &&
                        is.sbumpc() == 'l' &&
                        is.sbumpc() == 'l';
            } else {
                ref = Type{};

                return json(*ref).read(is);
            }
        }
#endif
    };

    struct json_write_options {
        constexpr json_write_options(size_t indent = 0, size_t current_indentation = 0) noexcept
            : current_indentation(current_indentation)
            , indent(indent)
        {}

        json_write_options indented() const noexcept {
            return { indent, current_indentation + indent };
        }

        size_t current_indentation; // Current indentation depth in number of spaces
        size_t indent; // Indent per level in number of spaces (0 if no indent desired)
    };

    template<typename Type>
    json_writer<Type> json(const Type &, json_write_options options = {});

    template<typename Type>
    class json_writer {
        const Type &ref;
        const json_write_options options;

        template<typename StreamChar>
        bool do_indent(std::basic_streambuf<StreamChar> &os, size_t sz) const {
            if (os.sputc('\n') == std::char_traits<StreamChar>::eof())
                return false;

            for (size_t i = 0; i < sz; ++i)
                if (os.sputc(' ') == std::char_traits<StreamChar>::eof())
                    return false;

            return true;
        }

    public:
        constexpr json_writer(const json_reader<Type> &reader, json_write_options options = {})
            : ref(reader.ref)
            , options(options)
        {}
        constexpr json_writer(const Type &ref, json_write_options options = {})
            : ref(ref)
            , options(options)
        {}

        // User object overload, skate_json(stream, object, options)
        template<typename StreamChar, typename _ = Type, typename type_exists<decltype(skate_json(static_cast<std::basic_streambuf<StreamChar> &>(std::declval<std::basic_streambuf<StreamChar> &>()), std::declval<const _ &>(), std::declval<json_write_options>()))>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // Library user is responsible for creating valid JSON in the callback function
            return skate_json(os, ref, options);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(begin(std::declval<_>()))>::value &&
                                                                                 !is_string_base<_>::value &&
                                                                                 !is_map_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            size_t index = 0;

            if (os.sputc('[') == std::char_traits<StreamChar>::eof())
                return false;

            for (const auto &el: ref) {
                if (index && os.sputc(',') == std::char_traits<StreamChar>::eof())
                    return false;

                if (options.indent && !do_indent(os, options.current_indentation + options.indent))
                    return false;

                if (!json(el, options.indented()).write(os))
                    return false;

                ++index;
            }

            if (options.indent && index && !do_indent(os, options.current_indentation)) // Only indent if array is non-empty
                return false;

            if (os.sputc(']') == std::char_traits<StreamChar>::eof())
                return false;

            return true;
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            typedef typename base_type<decltype(begin(ref))>::type KeyValuePair;

            size_t index = 0;

            if (os.sputc('{') == std::char_traits<StreamChar>::eof())
                return false;

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (index && os.sputc(',') == std::char_traits<StreamChar>::eof())
                    return false;

                if (options.indent && !do_indent(os, options.current_indentation + options.indent))
                    return false;

                if (!json(key_of<KeyValuePair>{}(el), options.indented()).write(os))
                    return false;

                if (os.sputc(':') == std::char_traits<StreamChar>::eof() ||
                    (options.indent && os.sputc(' ') == std::char_traits<StreamChar>::eof()))
                    return false;

                if (!json(value_of<KeyValuePair>{}(el), options.indented()).write(os))
                    return false;

                ++index;
            }

            if (options.indent && index && !do_indent(os, options.current_indentation)) // Only indent if object is non-empty
                return false;

            if (os.sputc('}') == std::char_traits<StreamChar>::eof())
                return false;

            return true;
        }

        // String overload
        template<typename StreamChar,
                 typename _ = Type,
                 typename StringChar = typename std::remove_cv<typename std::remove_reference<decltype(*begin(std::declval<_>()))>::type>::type,
                 typename std::enable_if<type_exists<decltype(unicode_codepoint(std::declval<StringChar>()))>::value &&
                                         is_string_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            const size_t sz = std::distance(begin(ref), end(ref));

            if (os.sputc('"') == std::char_traits<StreamChar>::eof())
                return false;

            for (size_t i = 0; i < sz; ++i) {
                const auto ch = ref[i];

                switch (ch) {
                    case '"':  if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('"') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\\': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('\\') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\b': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('b') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\f': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('f') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\n': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('n') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\r': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('r') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\t': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('t') == std::char_traits<StreamChar>::eof()) return false; break;
                    default: {
                        // Is character a control character? If so, write it out as a Unicode constant
                        // All other ASCII characters just get passed through
                        if (ch >= 32 && ch < 0x80) {
                            if (os.sputc(ch) == std::char_traits<StreamChar>::eof())
                                return false;
                        } else {
                            // Read Unicode in from string
                            const unicode_codepoint codepoint = get_unicode<StringChar>{}(ref, sz, i);

                            // Then add as a \u codepoint (or two, if surrogates are needed)
                            const char alphabet[] = "0123456789abcdef";
                            unsigned int hi, lo;

                            switch (utf16surrogates(codepoint.value(), &hi, &lo)) {
                                case 2:
                                    if (os.sputc('\\') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc('u') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(alphabet[hi >> 12]) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(alphabet[(hi >> 8) & 0xf]) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(alphabet[(hi >> 4) & 0xf]) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(alphabet[hi & 0xf]) == std::char_traits<StreamChar>::eof())
                                        return false;
                                    // fallthrough
                                case 1:
                                    if (os.sputc('\\') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc('u') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(alphabet[lo >> 12]) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(alphabet[(lo >> 8) & 0xf]) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(alphabet[(lo >> 4) & 0xf]) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(alphabet[lo & 0xf]) == std::char_traits<StreamChar>::eof())
                                        return false;
                                    break;
                                default:
                                    return false;
                            }
                        }

                        break;
                    }
                }
            }

            if (os.sputc('"') == std::char_traits<StreamChar>::eof())
                return false;

            return true;
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            const StreamChar array[] = {'n', 'u', 'l', 'l'};
            return os.sputn(array, 4) == 4;
        }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (ref) {
                const StreamChar true_array[] = {'t', 'r', 'u', 'e'};
                return os.sputn(true_array, 4) == 4;
            } else {
                const StreamChar false_array[] = {'f', 'a', 'l', 's', 'e'};
                return os.sputn(false_array, 5) == 5;
            }
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            const std::string str = std::to_string(ref);
            for (const auto c: str)
                if (os.sputc(c) == std::char_traits<StreamChar>::eof())
                    return false;

            return true;
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // JSON doesn't support infinities or NAN
            return impl::write_float(os, ref, false, false);
        }

        // Smart pointer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_shared_ptr_base<_>::value ||
                                                                                 is_weak_ptr_base<_>::value ||
                                                                                 is_unique_ptr_base<_>::value ||
                                                                                 std::is_pointer<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (!ref) {
                const StreamChar array[] = {'n', 'u', 'l', 'l'};
                return os.sputn(array, 4) == 4;
            } else {
                return json(*ref, options).write(os);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_optional_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (!ref) {
                const StreamChar array[] = {'n', 'u', 'l', 'l'};
                return os.sputn(array, 4) == 4;
            } else {
                return json(*ref, options).write(os);
            }
        }
#endif
    };

    template<typename Type>
    json_reader<Type> json(Type &value) { return json_reader<Type>(value); }

    template<typename Type>
    json_writer<Type> json(const Type &value, json_write_options options) { return json_writer<Type>(value, options); }

    template<typename StreamChar, typename Type>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, json_reader<Type> value) {
        if (!is.rdbuf() || !value.read(*is.rdbuf()))
            is.setstate(std::ios_base::failbit);
        return is;
    }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, json_reader<Type> value) {
        return os << json_writer<Type>(value);
    }
    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, json_writer<Type> value) {
        if (!os.rdbuf() || !value.write(*os.rdbuf()))
            os.setstate(std::ios_base::failbit);
        return os;
    }

    template<typename Type>
    Type from_json(const std::string &s) {
        Type value;

        struct one_pass_readbuf : public std::streambuf {
            one_pass_readbuf(const char *buf, size_t size) {
                setg(const_cast<char *>(buf),
                     const_cast<char *>(buf),
                     const_cast<char *>(buf + size));
            }
        } buf{s.c_str(), s.size()};

        if (!json(value).read(buf))
            return {};

        return value;
    }

    template<typename Type>
    std::string to_json(const Type &value, json_write_options options = {}) {
        std::stringbuf buf;
        if (!json(value, options).write(buf))
            return {};

        return buf.str();
    }

    // JSON classes that allow serialization and deserialization
    template<typename String>
    class basic_json_array;

    template<typename String>
    class basic_json_object;

    template<typename String>
    class basic_json_value {
    public:
        typedef basic_json_array<String> array;
        typedef basic_json_object<String> object;

        enum type {
            NullType,
            BooleanType,
            FloatType,
            Int64Type,
            UInt64Type,
            StringType,
            ArrayType,
            ObjectType
        };

        basic_json_value() : t(NullType) { d.p = nullptr; }
        basic_json_value(const basic_json_value &other) : t(NullType) {
            switch (other.t) {
                default: break;
                case BooleanType: // fallthrough
                case FloatType:   // fallthrough
                case Int64Type:   // fallthrough
                case UInt64Type:  d = other.d; break;
                case StringType:  d.p = new String(*other.internal_string()); break;
                case ArrayType:   d.p = new array(*other.internal_array());   break;
                case ObjectType:  d.p = new object(*other.internal_object()); break;
            }

            t = other.t;
        }
        basic_json_value(basic_json_value &&other) noexcept : t(other.t) {
            d = other.d;
            other.t = NullType;
        }
        basic_json_value(array a) : t(NullType) {
            d.p = new array(std::move(a));
            t = ArrayType;
        }
        basic_json_value(object o) : t(NullType) {
            d.p = new object(std::move(o));
            t = ObjectType;
        }
        basic_json_value(bool b) : t(BooleanType) { d.b = b; }
        basic_json_value(String s) : t(NullType) {
            d.p = new String(std::move(s));
            t = StringType;
        }
        template<typename Ch = decltype(*begin(std::declval<String>()))>
        basic_json_value(const Ch *s) : t(NullType) {
            d.p = new String(s);
            t = StringType;
        }
        template<typename Ch = decltype(*begin(std::declval<String>()))>
        basic_json_value(const Ch *s, size_t len) : t(NullType) {
            d.p = new String(s, len);
            t = StringType;
        }
        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        basic_json_value(T v) : t(FloatType) {
            if (std::trunc(v) == v) {
                // Convert to integer if at all possible, since its waaaay faster
                if (v >= INT64_MIN && v <= INT64_MAX) {
                    t = Int64Type;
                    d.i = int64_t(v);
                } else if (v >= 0 && v <= UINT64_MAX) {
                    t = UInt64Type;
                    d.u = uint64_t(v);
                } else
                    d.n = v;
            } else
                d.n = v;
        }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, int>::type = 0>
        basic_json_value(T v) : t(Int64Type) { d.i = v; }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, int>::type = 0>
        basic_json_value(T v) : t(UInt64Type) { d.u = v; }
        ~basic_json_value() { clear(); }

        basic_json_value &operator=(const basic_json_value &other) {
            if (&other == this)
                return *this;

            create(other.t);

            switch (other.t) {
                default: break;
                case BooleanType: // fallthrough
                case FloatType:   // fallthrough
                case Int64Type:   // fallthrough
                case UInt64Type:  d = other.d; break;
                case StringType:  *internal_string() = *other.internal_string(); break;
                case ArrayType:    *internal_array() = *other.internal_array();  break;
                case ObjectType:  *internal_object() = *other.internal_object(); break;
            }

            return *this;
        }
        basic_json_value &operator=(basic_json_value &&other) noexcept {
            clear();

            d = other.d;
            t = other.t;
            other.t = NullType;

            return *this;
        }

        type current_type() const noexcept { return t; }
        bool is_null() const noexcept { return t == NullType; }
        bool is_bool() const noexcept { return t == BooleanType; }
        bool is_number() const noexcept { return t == FloatType || t == Int64Type || t == UInt64Type; }
        bool is_floating() const noexcept { return t == FloatType; }
        bool is_int64() const noexcept { return t == Int64Type; }
        bool is_uint64() const noexcept { return t == UInt64Type; }
        bool is_string() const noexcept { return t == StringType; }
        bool is_array() const noexcept { return t == ArrayType; }
        bool is_object() const noexcept { return t == ObjectType; }

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // Undefined if current_type() is not the correct type
        std::nullptr_t unsafe_get_null() const noexcept { return nullptr; }
        bool unsafe_get_bool() const noexcept { return d.b; }
        double unsafe_get_floating() const noexcept { return d.n; }
        int64_t unsafe_get_int64() const noexcept { return d.i; }
        uint64_t unsafe_get_uint64() const noexcept { return d.u; }
        const String &unsafe_get_string() const noexcept { return *internal_string(); }
        const array &unsafe_get_array() const noexcept { return *internal_array(); }
        const object &unsafe_get_object() const noexcept { return *internal_object(); }
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        std::nullptr_t &null_ref() { static std::nullptr_t null; clear(); return null; }
        bool &bool_ref() { create(BooleanType); return d.b; }
        double &number_ref() { create(FloatType); return d.n; }
        int64_t &int64_ref() { create(Int64Type); return d.i; }
        uint64_t &uint64_ref() { create(UInt64Type); return d.u; }
        String &string_ref() { create(StringType); return *internal_string(); }
        array &array_ref() { create(ArrayType); return *internal_array(); }
        object &object_ref() { create(ObjectType); return *internal_object(); }

        // Returns default_value if not the correct type, or, in the case of numeric types, if the type could not be converted due to range (loss of precision with floating <-> int is allowed)
        bool get_bool(bool default_value = false) const { return is_bool()? d.b: default_value; }
        double get_number(double default_value = 0.0) const { return is_number()? d.n: is_int64()? d.i: is_uint64()? d.u: default_value; }
        int64_t get_int64(int64_t default_value = 0) const { return is_int64()? d.i: (is_uint64() && d.u <= INT64_MAX)? int64_t(d.u): (is_floating() && d.n >= INT64_MIN && d.n <= INT64_MAX)? int64_t(std::trunc(d.n)): default_value; }
        uint64_t get_uint64(uint64_t default_value = 0) const { return is_uint64()? d.u: (is_int64() && d.i >= 0)? uint64_t(d.i): (is_floating() && d.n >= 0 && d.n <= UINT64_MAX)? uint64_t(std::trunc(d.n)): default_value; }
        String get_string(String default_value = {}) const { return is_string()? unsafe_get_string(): default_value; }
        array get_array(array default_value = {}) const { return is_array()? unsafe_get_array(): default_value; }
        object get_object(object default_value = {}) const { return is_object()? unsafe_get_object(): default_value; }

        // ---------------------------------------------------
        // Array helpers
        void reserve(size_t size) { array_ref().reserve(size); }
        void resize(size_t size) { array_ref().resize(size); }
        void push_back(basic_json_value v) { array_ref().push_back(std::move(v)); }
        void pop_back() { array_ref().pop_back(); }

        const basic_json_value &at(size_t index) const {
            static const basic_json_value null;

            if (!is_array() || index >= unsafe_get_array().size())
                return null;

            return unsafe_get_array()[index];
        }
        const basic_json_value &operator[](size_t index) const { return at(index); }
        basic_json_value &operator[](size_t index) {
            if (index >= array_ref().size())
                internal_array()->resize(index + 1);

            return (*internal_array())[index];
        }
        // ---------------------------------------------------

        // ---------------------------------------------------
        // Object helpers
        basic_json_value value(const String &key, basic_json_value default_value = {}) const {
            if (!is_object())
                return default_value;

            return unsafe_get_object().value(key, default_value);
        }
        const basic_json_value &operator[](const String &key) const {
            static const basic_json_value null;

            if (!is_object())
                return null;

            const auto it = unsafe_get_object().find(key);
            if (it == unsafe_get_object().end())
                return null;

            return it->second;
        }
        basic_json_value &operator[](const String &key) { return object_ref()[key]; }
        basic_json_value &operator[](String &&key) { return object_ref()[std::move(key)]; }
        // ---------------------------------------------------

        size_t size() const noexcept {
            switch (t) {
                default: return 0;
                case StringType: return internal_string()->size();
                case ArrayType:  return  internal_array()->size();
                case ObjectType: return internal_object()->size();
            }
        }

        void clear() noexcept {
            switch (t) {
                default: break;
                case StringType: delete internal_string(); break;
                case ArrayType:  delete  internal_array(); break;
                case ObjectType: delete internal_object(); break;
            }
            t = NullType;
        }

        bool operator==(const basic_json_value &other) const {
            if (t != other.t) {
                if (is_number() && other.is_number()) {
                    switch (t) {
                        default: break;
                        case FloatType:  return d.n == other.get_number();
                        case Int64Type:  return other.is_uint64()? (other.d.u <= INT64_MAX && int64_t(other.d.u) == d.i): (other.d.n >= INT64_MIN && other.d.n <= INT64_MAX && d.i == int64_t(other.d.n));
                        case UInt64Type: return other.is_int64()? (other.d.i >= 0 && uint64_t(other.d.i) == d.u): (other.d.n >= 0 && other.d.n <= UINT64_MAX && d.u == uint64_t(other.d.n));
                    }
                }

                return false;
            }

            switch (t) {
                default:          return true;
                case BooleanType: return d.b == other.d.b;
                case FloatType:   return d.n == other.d.n;
                case Int64Type:   return d.i == other.d.i;
                case UInt64Type:  return d.u == other.d.u;
                case StringType:  return *internal_string() == *other.internal_string();
                case ArrayType:   return  *internal_array() == *other.internal_array();
                case ObjectType:  return *internal_object() == *other.internal_object();
            }
        }
        bool operator!=(const basic_json_value &other) const { return !(*this == other); }

    private:
        const String *internal_string() const noexcept { return static_cast<const String *>(d.p); }
        String *internal_string() noexcept { return static_cast<String *>(d.p); }

        const array *internal_array() const noexcept { return static_cast<const array *>(d.p); }
        array *internal_array() noexcept { return static_cast<array *>(d.p); }

        const object *internal_object() const noexcept { return static_cast<const object *>(d.p); }
        object *internal_object() noexcept { return static_cast<object *>(d.p); }

        void create(type t) {
            if (t == this->t)
                return;

            clear();

            switch (t) {
                default: break;
                case BooleanType: d.b = false; break;
                case FloatType:   d.n = 0.0; break;
                case Int64Type:   d.i = 0; break;
                case UInt64Type:  d.u = 0; break;
                case StringType:  d.p = new String(); break;
                case ArrayType:   d.p = new array(); break;
                case ObjectType:  d.p = new object(); break;
            }

            this->t = t;
        }

        type t;

        union {
            bool b;
            double n;
            int64_t i;
            uint64_t u;
            void *p;
        } d;
    };

    template<typename String>
    class basic_json_array {
        typedef std::vector<basic_json_value<String>> array;

        array v;

    public:
        basic_json_array() {}
        basic_json_array(std::initializer_list<basic_json_value<String>> il) : v(std::move(il)) {}

        typedef typename array::const_iterator const_iterator;
        typedef typename array::iterator iterator;

        iterator begin() noexcept { return v.begin(); }
        iterator end() noexcept { return v.end(); }
        const_iterator begin() const noexcept { return v.begin(); }
        const_iterator end() const noexcept { return v.end(); }

        void erase(size_t index) { v.erase(v.begin() + index); }
        void insert(size_t before, basic_json_value<String> item) { v.insert(v.begin() + before, std::move(item)); }
        void push_back(basic_json_value<String> item) { v.push_back(std::move(item)); }
        void pop_back() noexcept { v.pop_back(); }

        const basic_json_value<String> &operator[](size_t index) const noexcept { return v[index]; }
        basic_json_value<String> &operator[](size_t index) noexcept { return v[index]; }

        void resize(size_t size) { v.resize(size); }
        void reserve(size_t size) { v.reserve(size); }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_json_array &other) const { return v == other.v; }
        bool operator!=(const basic_json_array &other) const { return !(*this == other); }
    };

    template<typename String>
    class basic_json_object {
        typedef std::map<String, basic_json_value<String>> object;

        object v;

    public:
        basic_json_object() {}
        basic_json_object(std::initializer_list<std::pair<String, basic_json_value<String>>> il) : v(std::move(il)) {}

        typedef typename object::const_iterator const_iterator;
        typedef typename object::iterator iterator;

        iterator begin() noexcept { return v.begin(); }
        iterator end() noexcept { return v.end(); }
        const_iterator begin() const noexcept { return v.begin(); }
        const_iterator end() const noexcept { return v.end(); }

        iterator find(const String &key) { return v.find(key); }
        const_iterator find(const String &key) const { return v.find(key); }
        void erase(const String &key) { v.erase(key); }
        template<typename K, typename V>
        void insert(K &&key, V &&value) { v.insert(std::forward<K>(key), std::forward<V>(value)); }

        basic_json_value<String> value(const String &key, basic_json_value<String> default_value = {}) const {
            const auto it = v.find(key);
            if (it == v.end())
                return default_value;

            return it->second;
        }
        template<typename K>
        basic_json_value<String> &operator[](K &&key) {
            const auto it = v.find(key);
            if (it != v.end())
                return it->second;

            return v.insert({std::forward<K>(key), typename object::mapped_type{}}).first->second;
        }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_json_object &other) const { return v == other.v; }
        bool operator!=(const basic_json_object &other) const { return !(*this == other); }
    };

    typedef basic_json_array<std::string> json_array;
    typedef basic_json_object<std::string> json_object;
    typedef basic_json_value<std::string> json_value;

    typedef basic_json_array<std::wstring> json_warray;
    typedef basic_json_object<std::wstring> json_wobject;
    typedef basic_json_value<std::wstring> json_wvalue;

    template<typename StreamChar, typename String>
    bool skate_json(std::basic_streambuf<StreamChar> &is, basic_json_value<String> &j) {
        if (!impl::skipws(is))
            return false;

        auto c = is.sgetc();

        switch (c) {
            case std::char_traits<StreamChar>::eof(): return false;
            case '"': return json(j.string_ref()).read(is);
            case '[': return json(j.array_ref()).read(is);
            case '{': return json(j.object_ref()).read(is);
            case 't': // fallthrough
            case 'f': return json(j.bool_ref()).read(is);
            case 'n': return json(j.null_ref()).read(is);
            case '0': // fallthrough
            case '1': // fallthrough
            case '2': // fallthrough
            case '3': // fallthrough
            case '4': // fallthrough
            case '5': // fallthrough
            case '6': // fallthrough
            case '7': // fallthrough
            case '8': // fallthrough
            case '9': // fallthrough
            case '-': {
                std::string temp;
                const bool negative = c == '-';
                bool floating = false;

                do {
                    temp.push_back(c);
                    floating |= c == '.' || c == 'e' || c == 'E';

                    c = is.snextc();
                } while (impl::isdigit(c) || c == '-' || c == '.' || c == 'e' || c == 'E' || c == '+');

                char *end;
                if (floating) {
                    j.number_ref() = strtod(temp.c_str(), &end);
                } else if (negative) {
                    errno = 0;
                    j.int64_ref() = strtoll(temp.c_str(), &end, 10);
                    if (errno == ERANGE || *end != 0)
                        j.number_ref() = strtod(temp.c_str(), &end);
                } else {
                    errno = 0;
                    j.uint64_ref() = strtoull(temp.c_str(), &end, 10);
                    if (errno == ERANGE || *end != 0)
                        j.number_ref() = strtod(temp.c_str(), &end);
                }

                return *end == 0;
            }
            default: return false;
        }
    }

    template<typename StreamChar, typename String>
    bool skate_json(std::basic_streambuf<StreamChar> &os, const basic_json_value<String> &j, json_write_options options) {
        switch (j.current_type()) {
            case j.NullType:    return json(j.unsafe_get_null(), options).write(os);
            case j.BooleanType: return json(j.unsafe_get_bool(), options).write(os);
            case j.FloatType:   return json(j.unsafe_get_floating(), options).write(os);
            case j.Int64Type:   return json(j.unsafe_get_int64(), options).write(os);
            case j.UInt64Type:  return json(j.unsafe_get_uint64(), options).write(os);
            case j.StringType:  return json(j.unsafe_get_string(), options).write(os);
            case j.ArrayType:   return json(j.unsafe_get_array(), options).write(os);
            case j.ObjectType:  return json(j.unsafe_get_object(), options).write(os);
            default:            return json(nullptr, options).write(os);
        }
    }

    template<typename StreamChar, typename String>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, basic_json_value<String> &j) {
        return is >> json(j);
    }

    template<typename StreamChar, typename String>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const basic_json_value<String> &j) {
        return os << json(j);
    }

    // Qt helpers
#ifdef QT_VERSION
    template<typename StreamChar>
    std::basic_istream<StreamChar> &skate_json(std::basic_istream<StreamChar> &is, QString &str) {
        std::wstring wstr;
        is >> skate::json(wstr);
        str = QString::fromStdWString(wstr);
        return is;
    }

    template<typename StreamChar>
    std::basic_ostream<StreamChar> &skate_json(std::basic_ostream<StreamChar> &os, const QString &str) {
        return os << skate::json(str.toStdWString());
    }
#endif
}

#endif // SKATE_JSON_H
