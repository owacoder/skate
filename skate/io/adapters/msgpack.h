/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_MSGPACK_H
#define SKATE_MSGPACK_H

#include "core.h"

namespace skate {
    template<typename Type>
    class msgpack_reader;

    template<typename Type>
    class msgpack_writer;

    template<typename Type>
    msgpack_reader<Type> msgpack(Type &);

    namespace impl {
        namespace msgpack {
            // C++11 doesn't have generic lambdas, so create a functor class that allows reading a tuple
            template<typename StreamChar>
            class read_tuple {
                std::basic_streambuf<StreamChar> &is;
                bool &error;
                bool &has_read_something;

            public:
                constexpr read_tuple(std::basic_streambuf<StreamChar> &stream, bool &error, bool &has_read_something) noexcept
                    : is(stream)
                    , error(error)
                    , has_read_something(has_read_something)
                {}

                template<typename Param>
                void operator()(Param &p) {
                    if (error || (has_read_something && (!skate::impl::skipws(is) || is.sbumpc() != ','))) {
                        error = true;
                        return;
                    }

                    has_read_something = true;
                    error = !skate::msgpack(p).read(is);
                }
            };
        }
    }

    template<typename Type>
    class msgpack_reader {
        Type &ref;

        template<typename> friend class msgpack_writer;

    public:
        constexpr msgpack_reader(Type &ref) : ref(ref) {}

        // User object overload, skate_to_msgpack(stream, object)
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(skate_msgpack(std::declval<std::basic_streambuf<StreamChar> &>(), std::declval<_ &>()))>::value &&
                                                                                 !is_string_base<_>::value &&
                                                                                 !is_array_base<_>::value &&
                                                                                 !is_map_base<_>::value &&
                                                                                 !is_tuple_base<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            // Library user is responsible for validating read MsgPack in the callback function
            return skate_msgpack(is, ref);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array_base<_>::value &&
                                                                                 !is_tuple_base<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(*begin(std::declval<_>()))>::type Element;

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

                if (!msgpack(element).read(is))
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

        // Tuple/pair overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_tuple_base<_>::value &&
                                                                                 !is_array_base<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            bool error = false;
            bool has_read_something = false;

            if (!impl::skipws(is) || is.sbumpc() != '[')
                return false;

            impl::apply(impl::msgpack::read_tuple<StreamChar>(is, error, has_read_something), ref);

            return !error && impl::skipws(is) && is.sbumpc() == ']';
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(begin(std::declval<_>()))>::type KeyValuePair;
            typedef typename std::decay<typename is_map_pair_helper<KeyValuePair>::key_type>::type Key;
            typedef typename std::decay<typename is_map_pair_helper<KeyValuePair>::value_type>::type Value;

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

                if (!msgpack(key).read(is))
                    return false;

                if (!impl::skipws(is) || is.sbumpc() != ':')
                    return false;

                if (!msgpack(value).read(is))
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
                                                                                             std::declval<put_unicode<typename std::decay<decltype(*begin(std::declval<_>()))>::type>>()(std::declval<_ &>(), std::declval<unicode_codepoint>())
                                                                                             )>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            // Underlying char type of string
            typedef typename std::decay<decltype(*begin(std::declval<_>()))>::type StringChar;

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
                                const int digit = toxdigit(digits[i]);
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
                                    const int digit = toxdigit(digits[i]);
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
            ref = 0;
            if (!impl::skipws(is))
                return false;

            return impl::read_int(is, ref);
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref = 0;
            if (!impl::skipws(is))
                return false;

            return impl::read_float(is, ref, false, false);
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

                return msgpack(*ref).read(is);
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

                return msgpack(*ref).read(is);
            }
        }
#endif
    };

    struct msgpack_write_options {
        constexpr msgpack_write_options(size_t indent = 0, size_t current_indentation = 0) noexcept
            : current_indentation(current_indentation)
            , indent(indent)
        {}

        msgpack_write_options indented() const noexcept {
            return { indent, current_indentation + indent };
        }

        size_t current_indentation; // Current indentation depth in number of spaces
        size_t indent; // Indent per level in number of spaces (0 if no indent desired)
    };

    template<typename Type>
    msgpack_writer<Type> msgpack(const Type &, msgpack_write_options options = {});

    namespace impl {
        namespace msgpack {
            // C++11 doesn't have generic lambdas, so create a functor class that allows writing a tuple
            template<typename StreamChar>
            class write_tuple {
                std::basic_streambuf<StreamChar> &os;
                bool &error;
                const msgpack_write_options &options;

            public:
                constexpr write_tuple(std::basic_streambuf<StreamChar> &stream, bool &error, const msgpack_write_options &options) noexcept
                    : os(stream)
                    , error(error)
                    , options(options)
                {}

                template<typename Param>
                void operator()(const Param &p) {
                    if (error)
                        return;

                    error = !skate::msgpack(p, options).write(os);
                }
            };
        }
    }

    template<typename Type>
    class msgpack_writer {
        const Type &ref;
        const msgpack_write_options options;

    public:
        constexpr msgpack_writer(const msgpack_reader<Type> &reader, msgpack_write_options options = {})
            : ref(reader.ref)
            , options(options)
        {}
        constexpr msgpack_writer(const Type &ref, msgpack_write_options options = {})
            : ref(ref)
            , options(options)
        {}

        // User object overload, skate_msgpack(stream, object, options)
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(skate_msgpack(static_cast<std::basic_streambuf<StreamChar> &>(std::declval<std::basic_streambuf<StreamChar> &>()), std::declval<const _ &>(), std::declval<msgpack_write_options>()))>::value &&
                                                                                 !is_string_base<_>::value &&
                                                                                 !is_array_base<_>::value &&
                                                                                 !is_map_base<_>::value &&
                                                                                 !is_tuple_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // Library user is responsible for creating valid MsgPack in the callback function
            return skate_msgpack(os, ref, options);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (ref.size() <= 15) {
                if (os.sputc(0x90 | ref.size()) == std::char_traits<StreamChar>::eof())
                    return false;
            } else if (ref.size() <= 0xffffu) {
                if (os.sputc(0xdc) == std::char_traits<StreamChar>::eof() || !impl::write_big_endian(os, uint16_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffffffu) {
                if (os.sputc(0xdd) == std::char_traits<StreamChar>::eof() || !impl::write_big_endian(os, uint32_t(ref.size())))
                    return false;
            } else {
                return false;
            }

            for (const auto &el: ref) {
                if (!msgpack(el, options.indented()).write(os))
                    return false;
            }

            return true;
        }

        // Tuple/pair overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_tuple_base<_>::value &&
                                                                                 !is_array_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            constexpr size_t size = std::tuple_size<typename std::decay<_>::type>::value;
            bool error = false;

            if (size <= 15) {
                if (os.sputc(0x90 | size) == std::char_traits<StreamChar>::eof())
                    return false;
            } else if (size <= 0xffffu) {
                if (os.sputc(0xdc) == std::char_traits<StreamChar>::eof() || !impl::write_big_endian(os, uint16_t(size)))
                    return false;
            } else if (size <= 0xffffffffu) {
                if (os.sputc(0xdd) == std::char_traits<StreamChar>::eof() || !impl::write_big_endian(os, uint32_t(size)))
                    return false;
            } else {
                return false;
            }

            impl::apply(impl::msgpack::write_tuple<StreamChar>(os, error, options), ref);

            return !error;
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            typedef typename std::decay<decltype(begin(ref))>::type KeyValuePair;

            if (ref.size() <= 15) {
                if (os.sputc(0x80 | ref.size()) == std::char_traits<StreamChar>::eof())
                    return false;
            } else if (ref.size() <= 0xffffu) {
                if (os.sputc(0xde) == std::char_traits<StreamChar>::eof() || !impl::write_big_endian(os, uint16_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffffffu) {
                if (os.sputc(0xdf) == std::char_traits<StreamChar>::eof() || !impl::write_big_endian(os, uint32_t(ref.size())))
                    return false;
            } else {
                return false;
            }

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (!msgpack(key_of<KeyValuePair>{}(el), options.indented()).write(os))
                    return false;

                if (!msgpack(value_of<KeyValuePair>{}(el), options.indented()).write(os))
                    return false;
            }

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

            if (sz <= 31) {
                if (os.sputc(0xa0 | sz) == std::char_traits<StreamChar>::eof())
                    return false;
            } else if (sz <= 0xffu) {
                if (os.sputc(0xd9) == std::char_traits<StreamChar>::eof() || !impl::write_big_endian(os, uint8_t(sz)))
                    return false;
            } else if (sz <= 0xffffu) {
                if (os.sputc(0xda) == std::char_traits<StreamChar>::eof() || !impl::write_big_endian(os, uint16_t(sz)))
                    return false;
            } else if (sz <= 0xffffffffu) {
                if (os.sputc(0xdb) == std::char_traits<StreamChar>::eof() || !impl::write_big_endian(os, uint32_t(sz)))
                    return false;
            } else {
                return false;
            }

            for (size_t i = 0; i < sz;) {
                // Read Unicode in from string
                const unicode_codepoint codepoint = get_unicode<StringChar>{}(ref, sz, i);

                switch (codepoint.value()) {
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
                        if (codepoint >= 32 && codepoint < 0x80) {
                            if (os.sputc(codepoint.value()) == std::char_traits<StreamChar>::eof())
                                return false;
                        } else {
                            // Then add as a \u codepoint (or two, if surrogates are needed)
                            unsigned int hi, lo;

                            switch (utf16surrogates(codepoint.value(), &hi, &lo)) {
                                case 2:
                                    if (os.sputc('\\') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc('u') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(hi >> 12)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(hi >>  8)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(hi >>  4)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(hi      )) == std::char_traits<StreamChar>::eof())
                                        return false;
                                    // fallthrough
                                case 1:
                                    if (os.sputc('\\') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc('u') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(lo >> 12)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(lo >>  8)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(lo >>  4)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(lo      )) == std::char_traits<StreamChar>::eof())
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

            return os.sputc('"') != std::char_traits<StreamChar>::eof();
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            return os.sputc(0xc0) != std::char_traits<StreamChar>::eof();
        }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            return os.sputc(0xc2 | ref) != std::char_traits<StreamChar>::eof();
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (ref < 0) {
                if (ref >= -32) {
                    // TODO
                }

                // TODO
            } else { // Positive number
                if (ref <= 0x7fu)
                    return os.sputc(ref) != std::char_traits<StreamChar>::eof();
                else if (ref <= 0xffu)
                    return os.sputc(0xcc) != std::char_traits<StreamChar>::eof() && impl::write_big_endian(os, uint8_t(ref));
                else if (ref <= 0xffffu)
                    return os.sputc(0xcd) != std::char_traits<StreamChar>::eof() && impl::write_big_endian(os, uint16_t(ref));
                else if (ref <= 0xffffffffu)
                    return os.sputc(0xce) != std::char_traits<StreamChar>::eof() && impl::write_big_endian(os, uint32_t(ref));
                else
                    return os.sputc(0xcf) != std::char_traits<StreamChar>::eof() && impl::write_big_endian(os, uint64_t(ref));
            }
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // MsgPack doesn't support infinities or NAN
            return impl::write_float(os, ref, false, false);
        }

        // Smart pointer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<(is_shared_ptr_base<_>::value ||
                                                                                 is_weak_ptr_base<_>::value ||
                                                                                 is_unique_ptr_base<_>::value ||
                                                                                 std::is_pointer<_>::value) && !is_string_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (!ref) {
                return os.sputc(0xc0) != std::char_traits<StreamChar>::eof();
            } else {
                return msgpack(*ref, options).write(os);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_optional_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (!ref) {
                return os.sputc(0xc0) != std::char_traits<StreamChar>::eof();
            } else {
                return msgpack(*ref, options).write(os);
            }
        }
#endif
    };

    template<typename Type>
    msgpack_reader<Type> msgpack(Type &value) { return msgpack_reader<Type>(value); }

    template<typename Type>
    msgpack_writer<Type> msgpack(const Type &value, msgpack_write_options options) { return msgpack_writer<Type>(value, options); }

    template<typename StreamChar, typename Type>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, msgpack_reader<Type> value) {
        if (!is.rdbuf() || !value.read(*is.rdbuf()))
            is.setstate(std::ios_base::failbit);
        return is;
    }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, msgpack_reader<Type> value) {
        return os << msgpack_writer<Type>(value);
    }
    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, msgpack_writer<Type> value) {
        if (!os.rdbuf() || !value.write(*os.rdbuf()))
            os.setstate(std::ios_base::failbit);
        return os;
    }

    template<typename Type>
    Type from_msgpack(const std::string &s) {
        Type value;

        struct one_pass_readbuf : public std::streambuf {
            one_pass_readbuf(const char *buf, size_t size) {
                setg(const_cast<char *>(buf),
                     const_cast<char *>(buf),
                     const_cast<char *>(buf + size));
            }
        } buf{s.c_str(), s.size()};

        if (!msgpack(value).read(buf))
            return {};

        return value;
    }

    template<typename Type>
    std::string to_msgpack(const Type &value, msgpack_write_options options = {}) {
        std::stringbuf buf;
        if (!msgpack(value, options).write(buf))
            return {};

        return buf.str();
    }

    // MsgPack classes that allow serialization and deserialization
    template<typename String>
    class basic_msgpack_array;

    template<typename String>
    class basic_msgpack_object;

    enum class msgpack_type {
        null,
        boolean,
        floating,
        int64,
        uint64,
        string,
        array,
        object
    };

    template<typename String>
    class basic_msgpack_value {
    public:
        typedef basic_msgpack_array<String> array;
        typedef basic_msgpack_object<String> object;

        basic_msgpack_value() : t(msgpack_type::null) { d.p = nullptr; }
        basic_msgpack_value(const basic_msgpack_value &other) : t(msgpack_type::null) {
            switch (other.t) {
                default: break;
                case msgpack_type::boolean:    // fallthrough
                case msgpack_type::floating:   // fallthrough
                case msgpack_type::int64:      // fallthrough
                case msgpack_type::uint64:     d = other.d; break;
                case msgpack_type::string:     d.p = new String(*other.internal_string()); break;
                case msgpack_type::array:      d.p = new array(*other.internal_array());   break;
                case msgpack_type::object:     d.p = new object(*other.internal_object()); break;
            }

            t = other.t;
        }
        basic_msgpack_value(basic_msgpack_value &&other) noexcept : t(other.t) {
            d = other.d;
            other.t = msgpack_type::null;
        }
        basic_msgpack_value(array a) : t(msgpack_type::null) {
            d.p = new array(std::move(a));
            t = msgpack_type::array;
        }
        basic_msgpack_value(object o) : t(msgpack_type::null) {
            d.p = new object(std::move(o));
            t = msgpack_type::object;
        }
        basic_msgpack_value(bool b) : t(msgpack_type::boolean) { d.b = b; }
        basic_msgpack_value(String s) : t(msgpack_type::null) {
            d.p = new String(std::move(s));
            t = msgpack_type::string;
        }
        basic_msgpack_value(const typename std::remove_reference<decltype(*begin(std::declval<String>()))>::type *s) : t(msgpack_type::string) {
            d.p = new String(s);
        }
        basic_msgpack_value(const typename std::remove_reference<decltype(*begin(std::declval<String>()))>::type *s, size_t len) : t(msgpack_type::string) {
            d.p = new String(s, len);
        }
        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        basic_msgpack_value(T v) : t(msgpack_type::floating) {
            if (std::trunc(v) == v) {
                // Convert to integer if at all possible, since its waaaay faster
                if (v >= INT64_MIN && v <= INT64_MAX) {
                    t = msgpack_type::int64;
                    d.i = int64_t(v);
                } else if (v >= 0 && v <= UINT64_MAX) {
                    t = msgpack_type::uint64;
                    d.u = uint64_t(v);
                } else
                    d.n = v;
            } else
                d.n = v;
        }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, int>::type = 0>
        basic_msgpack_value(T v) : t(msgpack_type::int64) { d.i = v; }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, int>::type = 0>
        basic_msgpack_value(T v) : t(msgpack_type::uint64) { d.u = v; }
        template<typename T, typename std::enable_if<is_string_base<T>::value, int>::type = 0>
        basic_msgpack_value(const T &v) : t(msgpack_type::string) {
            d.p = new String(std::move(utf_convert<String>(v)));
        }
        ~basic_msgpack_value() { clear(); }

        basic_msgpack_value &operator=(const basic_msgpack_value &other) {
            if (&other == this)
                return *this;

            create(other.t);

            switch (other.t) {
                default: break;
                case msgpack_type::boolean:     // fallthrough
                case msgpack_type::floating:    // fallthrough
                case msgpack_type::int64:       // fallthrough
                case msgpack_type::uint64:      d = other.d; break;
                case msgpack_type::string:      *internal_string() = *other.internal_string(); break;
                case msgpack_type::array:       *internal_array() = *other.internal_array();  break;
                case msgpack_type::object:      *internal_object() = *other.internal_object(); break;
            }

            return *this;
        }
        basic_msgpack_value &operator=(basic_msgpack_value &&other) noexcept {
            clear();

            d = other.d;
            t = other.t;
            other.t = msgpack_type::null;

            return *this;
        }

        msgpack_type current_type() const noexcept { return t; }
        bool is_null() const noexcept { return t == msgpack_type::null; }
        bool is_bool() const noexcept { return t == msgpack_type::boolean; }
        bool is_number() const noexcept { return t == msgpack_type::floating || t == msgpack_type::int64 || t == msgpack_type::uint64; }
        bool is_floating() const noexcept { return t == msgpack_type::floating; }
        bool is_int64() const noexcept { return t == msgpack_type::int64; }
        bool is_uint64() const noexcept { return t == msgpack_type::uint64; }
        bool is_string() const noexcept { return t == msgpack_type::string; }
        bool is_array() const noexcept { return t == msgpack_type::array; }
        bool is_object() const noexcept { return t == msgpack_type::object; }

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
        bool &bool_ref() { create(msgpack_type::boolean); return d.b; }
        double &number_ref() { create(msgpack_type::floating); return d.n; }
        int64_t &int64_ref() { create(msgpack_type::int64); return d.i; }
        uint64_t &uint64_ref() { create(msgpack_type::uint64); return d.u; }
        String &string_ref() { create(msgpack_type::string); return *internal_string(); }
        array &array_ref() { create(msgpack_type::array); return *internal_array(); }
        object &object_ref() { create(msgpack_type::object); return *internal_object(); }

        // Returns default_value if not the correct type, or, in the case of numeric types, if the type could not be converted due to range (loss of precision with floating <-> int is allowed)
        bool get_bool(bool default_value = false) const { return is_bool()? d.b: default_value; }
        double get_number(double default_value = 0.0) const { return is_number()? d.n: is_int64()? d.i: is_uint64()? d.u: default_value; }
        int64_t get_int64(int64_t default_value = 0) const { return is_int64()? d.i: (is_uint64() && d.u <= INT64_MAX)? int64_t(d.u): (is_floating() && d.n >= INT64_MIN && d.n <= INT64_MAX)? int64_t(std::trunc(d.n)): default_value; }
        uint64_t get_uint64(uint64_t default_value = 0) const { return is_uint64()? d.u: (is_int64() && d.i >= 0)? uint64_t(d.i): (is_floating() && d.n >= 0 && d.n <= UINT64_MAX)? uint64_t(std::trunc(d.n)): default_value; }
        String get_string(String default_value = {}) const { return is_string()? unsafe_get_string(): default_value; }
        template<typename S>
        S get_string(S default_value = {}) const { return is_string()? utf_convert<S>(unsafe_get_string()): default_value; }
        array get_array(array default_value = {}) const { return is_array()? unsafe_get_array(): default_value; }
        object get_object(object default_value = {}) const { return is_object()? unsafe_get_object(): default_value; }

        // ---------------------------------------------------
        // Array helpers
        void reserve(size_t size) { array_ref().reserve(size); }
        void resize(size_t size) { array_ref().resize(size); }
        void push_back(basic_msgpack_value v) { array_ref().push_back(std::move(v)); }
        void pop_back() { array_ref().pop_back(); }

        const basic_msgpack_value &at(size_t index) const {
            static const basic_msgpack_value null;

            if (!is_array() || index >= unsafe_get_array().size())
                return null;

            return unsafe_get_array()[index];
        }
        const basic_msgpack_value &operator[](size_t index) const { return at(index); }
        basic_msgpack_value &operator[](size_t index) {
            if (index >= array_ref().size())
                internal_array()->resize(index + 1);

            return (*internal_array())[index];
        }
        // ---------------------------------------------------

        // ---------------------------------------------------
        // Object helpers
        basic_msgpack_value value(const String &key, basic_msgpack_value default_value = {}) const {
            if (!is_object())
                return default_value;

            return unsafe_get_object().value(key, default_value);
        }
        template<typename S, typename std::enable_if<is_string_base<S>::value, int>::type = 0>
        basic_msgpack_value value(const S &key, basic_msgpack_value default_value = {}) const {
            return value(utf_convert<String>(key), default_value);
        }

        const basic_msgpack_value &operator[](const String &key) const {
            static const basic_msgpack_value null;

            if (!is_object())
                return null;

            const auto it = unsafe_get_object().find(key);
            if (it == unsafe_get_object().end())
                return null;

            return it->second;
        }
        basic_msgpack_value &operator[](const String &key) { return object_ref()[key]; }
        basic_msgpack_value &operator[](String &&key) { return object_ref()[std::move(key)]; }
        template<typename S, typename std::enable_if<is_string_base<S>::value, int>::type = 0>
        const basic_msgpack_value &operator[](const S &key) const {
            return (*this)[utf_convert<String>(key)];
        }
        template<typename S, typename std::enable_if<is_string_base<S>::value, int>::type = 0>
        basic_msgpack_value &operator[](const S &key) {
            return (*this)[std::move(utf_convert<String>(key))];
        }
        // ---------------------------------------------------

        size_t size() const noexcept {
            switch (t) {
                default: return 0;
                case msgpack_type::string: return internal_string()->size();
                case msgpack_type::array:  return  internal_array()->size();
                case msgpack_type::object: return internal_object()->size();
            }
        }

        void clear() noexcept {
            switch (t) {
                default: break;
                case msgpack_type::string: delete internal_string(); break;
                case msgpack_type::array:  delete  internal_array(); break;
                case msgpack_type::object: delete internal_object(); break;
            }

            t = msgpack_type::null;
        }

        bool operator==(const basic_msgpack_value &other) const {
            if (t != other.t) {
                if (is_number() && other.is_number()) {
                    switch (t) {
                        default: break;
                        case msgpack_type::floating:  return d.n == other.get_number();
                        case msgpack_type::int64:     return other.is_uint64()? (other.d.u <= INT64_MAX && int64_t(other.d.u) == d.i): (other.d.n >= INT64_MIN && other.d.n <= INT64_MAX && d.i == int64_t(other.d.n));
                        case msgpack_type::uint64:    return other.is_int64()? (other.d.i >= 0 && uint64_t(other.d.i) == d.u): (other.d.n >= 0 && other.d.n <= UINT64_MAX && d.u == uint64_t(other.d.n));
                    }
                }

                return false;
            }

            switch (t) {
                default:                     return true;
                case msgpack_type::boolean:     return d.b == other.d.b;
                case msgpack_type::floating:    return d.n == other.d.n;
                case msgpack_type::int64:       return d.i == other.d.i;
                case msgpack_type::uint64:      return d.u == other.d.u;
                case msgpack_type::string:      return *internal_string() == *other.internal_string();
                case msgpack_type::array:       return  *internal_array() == *other.internal_array();
                case msgpack_type::object:      return *internal_object() == *other.internal_object();
            }
        }
        bool operator!=(const basic_msgpack_value &other) const { return !(*this == other); }

    private:
        const String *internal_string() const noexcept { return static_cast<const String *>(d.p); }
        String *internal_string() noexcept { return static_cast<String *>(d.p); }

        const array *internal_array() const noexcept { return static_cast<const array *>(d.p); }
        array *internal_array() noexcept { return static_cast<array *>(d.p); }

        const object *internal_object() const noexcept { return static_cast<const object *>(d.p); }
        object *internal_object() noexcept { return static_cast<object *>(d.p); }

        void create(msgpack_type t) {
            if (t == this->t)
                return;

            clear();

            switch (t) {
                default: break;
                case msgpack_type::boolean:     d.b = false; break;
                case msgpack_type::floating:    d.n = 0.0; break;
                case msgpack_type::int64:       d.i = 0; break;
                case msgpack_type::uint64:      d.u = 0; break;
                case msgpack_type::string:      d.p = new String(); break;
                case msgpack_type::array:       d.p = new array(); break;
                case msgpack_type::object:      d.p = new object(); break;
            }

            this->t = t;
        }

        msgpack_type t;

        union {
            bool b;
            double n;
            int64_t i;
            uint64_t u;
            void *p;
        } d;
    };

    template<typename String>
    class basic_msgpack_array {
        typedef std::vector<basic_msgpack_value<String>> array;

        array v;

    public:
        basic_msgpack_array() {}
        basic_msgpack_array(std::initializer_list<basic_msgpack_value<String>> il) : v(std::move(il)) {}

        typedef typename array::const_iterator const_iterator;
        typedef typename array::iterator iterator;

        iterator begin() noexcept { return v.begin(); }
        iterator end() noexcept { return v.end(); }
        const_iterator begin() const noexcept { return v.begin(); }
        const_iterator end() const noexcept { return v.end(); }

        void erase(size_t index) { v.erase(v.begin() + index); }
        void insert(size_t before, basic_msgpack_value<String> item) { v.insert(v.begin() + before, std::move(item)); }
        void push_back(basic_msgpack_value<String> item) { v.push_back(std::move(item)); }
        void pop_back() noexcept { v.pop_back(); }

        const basic_msgpack_value<String> &operator[](size_t index) const noexcept { return v[index]; }
        basic_msgpack_value<String> &operator[](size_t index) noexcept { return v[index]; }

        void resize(size_t size) { v.resize(size); }
        void reserve(size_t size) { v.reserve(size); }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_msgpack_array &other) const { return v == other.v; }
        bool operator!=(const basic_msgpack_array &other) const { return !(*this == other); }
    };

    template<typename String>
    class basic_msgpack_object {
        typedef std::map<String, basic_msgpack_value<String>> object;

        object v;

    public:
        basic_msgpack_object() {}
        basic_msgpack_object(std::initializer_list<std::pair<const String, basic_msgpack_value<String>>> il) : v(std::move(il)) {}

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

        basic_msgpack_value<String> value(const String &key, basic_msgpack_value<String> default_value = {}) const {
            const auto it = v.find(key);
            if (it == v.end())
                return default_value;

            return it->second;
        }
        template<typename S, typename std::enable_if<is_string_base<S>::value, int>::type = 0>
        basic_msgpack_value<String> value(const S &key, basic_msgpack_value<String> default_value = {}) const {
            return value(utf_convert<String>(key), default_value);
        }

        basic_msgpack_value<String> &operator[](const String &key) {
            const auto it = v.find(key);
            if (it != v.end())
                return it->second;

            return v.insert({key, typename object::mapped_type{}}).first->second;
        }
        basic_msgpack_value<String> &operator[](String &&key) {
            const auto it = v.find(key);
            if (it != v.end())
                return it->second;

            return v.insert({std::move(key), typename object::mapped_type{}}).first->second;
        }
        template<typename S, typename std::enable_if<is_string_base<S>::value, int>::type = 0>
        const basic_msgpack_value<String> &operator[](const S &key) const {
            return (*this)[utf_convert<String>(key)];
        }
        template<typename S, typename std::enable_if<is_string_base<S>::value, int>::type = 0>
        basic_msgpack_value<String> &operator[](const S &key) {
            return (*this)[std::move(utf_convert<String>(key))];
        }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_msgpack_object &other) const { return v == other.v; }
        bool operator!=(const basic_msgpack_object &other) const { return !(*this == other); }
    };

    typedef basic_msgpack_array<std::string> msgpack_array;
    typedef basic_msgpack_object<std::string> msgpack_object;
    typedef basic_msgpack_value<std::string> msgpack_value;

    typedef basic_msgpack_array<std::wstring> msgpack_warray;
    typedef basic_msgpack_object<std::wstring> msgpack_wobject;
    typedef basic_msgpack_value<std::wstring> msgpack_wvalue;

    template<typename StreamChar, typename String>
    bool skate_msgpack(std::basic_streambuf<StreamChar> &is, basic_msgpack_value<String> &j) {
        if (!impl::skipws(is))
            return false;

        auto c = is.sgetc();

        switch (c) {
            case std::char_traits<StreamChar>::eof(): return false;
            case '"': return msgpack(j.string_ref()).read(is);
            case '[': return msgpack(j.array_ref()).read(is);
            case '{': return msgpack(j.object_ref()).read(is);
            case 't': // fallthrough
            case 'f': return msgpack(j.bool_ref()).read(is);
            case 'n': return msgpack(j.null_ref()).read(is);
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
                } while (isfpdigit(c));

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
    bool skate_msgpack(std::basic_streambuf<StreamChar> &os, const basic_msgpack_value<String> &j, msgpack_write_options options) {
        switch (j.current_type()) {
            default:                         return msgpack(nullptr, options).write(os);
            case msgpack_type::null:         return msgpack(j.unsafe_get_null(), options).write(os);
            case msgpack_type::boolean:      return msgpack(j.unsafe_get_bool(), options).write(os);
            case msgpack_type::floating:     return msgpack(j.unsafe_get_floating(), options).write(os);
            case msgpack_type::int64:        return msgpack(j.unsafe_get_int64(), options).write(os);
            case msgpack_type::uint64:       return msgpack(j.unsafe_get_uint64(), options).write(os);
            case msgpack_type::string:       return msgpack(j.unsafe_get_string(), options).write(os);
            case msgpack_type::array:        return msgpack(j.unsafe_get_array(), options).write(os);
            case msgpack_type::object:       return msgpack(j.unsafe_get_object(), options).write(os);
        }
    }

    template<typename StreamChar, typename String>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, basic_msgpack_value<String> &j) {
        return is >> msgpack(j);
    }

    template<typename StreamChar, typename String>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const basic_msgpack_value<String> &j) {
        return os << msgpack(j);
    }

    // Qt helpers
#ifdef QT_VERSION
    template<typename StreamChar>
    std::basic_istream<StreamChar> &skate_msgpack(std::basic_istream<StreamChar> &is, QString &str) {
        std::wstring wstr;
        is >> skate::msgpack(wstr);
        str = QString::fromStdWString(wstr);
        return is;
    }

    template<typename StreamChar>
    std::basic_ostream<StreamChar> &skate_msgpack(std::basic_ostream<StreamChar> &os, const QString &str) {
        return os << skate::msgpack(str.toStdWString());
    }
#endif
}

#endif // SKATE_MSGPACK_H
