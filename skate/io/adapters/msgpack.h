/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_MSGPACK_H
#define SKATE_MSGPACK_H

#include "core.h"
#include "../../system/time.h"

namespace skate {
    struct msgpack_ext {
        msgpack_ext() : type(0) {}

        int8_t type;
        std::string data;
    };

    struct msgpack_options {
        constexpr msgpack_options() : strict_signed_int(false), strict_size(false) {}

        bool strict_signed_int; // Strict interpretation of signed/unsigned types (write-only)
        bool strict_size; // Strict interpretation of integer/floating-point size (write-only)
    };

    template<typename Type>
    class msgpack_reader;

    template<typename Type>
    class msgpack_writer;

    template<typename Type>
    msgpack_reader<Type> msgpack(Type &, msgpack_options options = {});

    namespace impl {
        namespace msgpack {
            // C++11 doesn't have generic lambdas, so create a functor class that allows reading a tuple
            class read_tuple {
                std::streambuf &is;
                bool &error;
                const msgpack_options &options;

            public:
                constexpr read_tuple(std::streambuf &stream, bool &error, const msgpack_options &options) noexcept
                    : is(stream)
                    , error(error)
                    , options(options)
                {}

                template<typename Param>
                void operator()(Param &p) {
                    if (error)
                        return;

                    error = !skate::msgpack(p, options).read(is);
                }
            };
        }
    }

    template<typename Type>
    class msgpack_reader {
        Type &ref;
        msgpack_options options;

        template<typename> friend class msgpack_writer;

        int sbump_byte(std::streambuf &is) {
            const auto c = is.sbumpc();
            if (c == std::char_traits<char>::eof())
                return -1;

            return static_cast<unsigned char>(c);
        }
        int sget_byte(std::streambuf &is) {
            const auto c = is.sgetc();
            if (c == std::char_traits<char>::eof())
                return -1;

            return static_cast<unsigned char>(c);
        }

    public:
        constexpr msgpack_reader(Type &ref, msgpack_options options = {}) : ref(ref), options(options) {}

        // User object overload, skate_to_msgpack(stream, object)
        template<typename _ = Type, typename std::enable_if<type_exists<decltype(skate_msgpack(std::declval<std::streambuf &>(), std::declval<_ &>()))>::value &&
                                                            !is_string_base<_>::value &&
                                                            !is_array_base<_>::value &&
                                                            !is_map_base<_>::value &&
                                                            !is_tuple_base<_>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            // Library user is responsible for validating read MsgPack in the callback function
            return skate_msgpack(is, ref);
        }

        // Array overload
        template<typename _ = Type, typename std::enable_if<is_array_base<_>::value &&
                                                            !is_tuple_base<_>::value &&
                                                            !std::is_convertible<typename std::decay<decltype(*begin(std::declval<_>()))>::type, char>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            typedef typename std::decay<decltype(*begin(std::declval<_>()))>::type Element;

            abstract::clear(ref);

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;

            switch (c) {
                case 0xdc: { /* 2-byte size array */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdd: { /* 4-byte size array */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size array? */
                    if (c >= 0 && (c & 0xf0) == 0x90)
                        size = c & 0xf;
                    else
                        return false;
                    break;
                }
            }

            abstract::reserve(ref, size);

            for (uint_fast32_t i = 0; i < size; ++i) {
                Element element;

                if (!msgpack(element).read(is))
                    return false;

                abstract::push_back(ref, std::move(element));
            }

            return true;
        }

        // Array overload (allows binary data)
        template<typename _ = Type, typename std::enable_if<is_array_base<_>::value &&
                                                            !is_tuple_base<_>::value &&
                                                            std::is_convertible<unsigned char, typename std::decay<decltype(*begin(std::declval<_>()))>::type>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            typedef typename std::decay<decltype(*begin(std::declval<_>()))>::type Element;

            abstract::clear(ref);

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;
            bool is_binary = false;

            switch (c) {
                case 0xc4: { /* 1-byte size binary */
                    uint8_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    is_binary = true;
                    break;
                }
                case 0xc5: { /* 2-byte size binary */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    is_binary = true;
                    break;
                }
                case 0xc6: { /* 4-byte size binary */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    is_binary = true;
                    break;
                }
                case 0xdc: { /* 2-byte size array */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdd: { /* 4-byte size array */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size array? */
                    if (c >= 0 && (c & 0xf0) == 0x90)
                        size = c & 0xf;
                    else
                        return false;
                    break;
                }
            }

            abstract::reserve(ref, size);

            if (is_binary) {
                for (uint_fast32_t i = 0; i < size; ++i) {
                    const int element = sget_byte(is);

                    if (element == std::char_traits<char>::eof())
                        return false;

                    abstract::push_back(ref, static_cast<unsigned char>(element));
                }
            } else {
                for (uint_fast32_t i = 0; i < size; ++i) {
                    Element element;

                    if (!msgpack(element).read(is))
                        return false;

                    abstract::push_back(ref, std::move(element));
                }
            }

            return true;
        }

        // Tuple/pair overload
        template<typename _ = Type, typename std::enable_if<is_tuple_base<_>::value &&
                                                            !is_array_base<_>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            bool error = false;

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;

            switch (c) {
                case 0xdc: { /* 2-byte size array */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdd: { /* 4-byte size array */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size array? */
                    if (c >= 0 && (c & 0xf0) == 0x90)
                        size = c & 0xf;
                    else
                        return false;
                    break;
                }
            }

            if (size != std::tuple_size<typename std::decay<_>::type>::value)
                return false;

            impl::apply(impl::msgpack::read_tuple(is, error, options), ref);

            return !error;
        }

        // Map overload
        template<typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            typedef typename std::decay<decltype(begin(std::declval<_>()))>::type KeyValuePair;
            typedef typename std::decay<typename is_map_pair_helper<KeyValuePair>::key_type>::type Key;
            typedef typename std::decay<typename is_map_pair_helper<KeyValuePair>::value_type>::type Value;

            abstract::clear(ref);

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;

            switch (c) {
                case 0xde: { /* 2-byte size map */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdf: { /* 4-byte size map */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size map? */
                    if (c >= 0 && (c & 0xf0) == 0x80)
                        size = c & 0xf;
                    else
                        return false;
                    break;
                }
            }

            for (uint_fast32_t i = 0; i < size; ++i) {
                Key key;
                Value value;

                if (!msgpack(key).read(is) ||
                    !msgpack(value).read(is))
                    return false;

                ref[std::move(key)] = std::move(value);
            }

            return true;
        }

        // String overload (TODO)
        template<typename _ = Type, typename std::enable_if<is_string_base<_>::value &&
                                                            type_exists<decltype(
                                                                        // Only if put_unicode is available
                                                                        std::declval<put_unicode<typename std::decay<decltype(*begin(std::declval<_>()))>::type>>()(std::declval<_ &>(), std::declval<unicode_codepoint>())
                                                                        )>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            // Underlying char type of string
            typedef typename std::decay<decltype(*begin(std::declval<_>()))>::type StringChar;

            abstract::clear(ref);

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;

            switch (c) {
                case 0xd9: { /* 1-byte size string */
                    uint8_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xda: { /* 2-byte size string */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdb: { /* 4-byte size string */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size string? */
                    if (c >= 0 && (c & 0xe0) == 0xa0)
                        size = c & 0x1f;
                    else
                        return false;
                    break;
                }
            }

            // Read start char
            if (!impl::skipws(is) || is.sbumpc() != '"')
                return false;

            while (get_unicode<char>{}(is, codepoint)) {
                // End of string?
                if (codepoint == '"') {
                    return true;
                }
                // Escaped character sequence?
                else if (codepoint == '\\') {
                    const auto c = is.sbumpc();

                    switch (c) {
                        case std::char_traits<char>::eof(): return false;
                        case '"':
                        case '\\':
                        case 'b':
                        case 'f':
                        case 'n':
                        case 'r':
                        case 't': codepoint = c; break;
                        case 'u': {
                            char digits[4] = { 0 };
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

            return false;
        }

        // Null overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            return sbump_byte(is) == 0xc0;
        }

        // Boolean overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            switch (sbump_byte(is)) {
                case 0xc2: ref = false; return true;
                case 0xc3: ref =  true; return true;
                default:   ref = false; return false;
            }
        }

        // Integer overload (TODO)
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref = 0;
            if (!impl::skipws(is))
                return false;

            return impl::read_int(is, ref);
        }

        // Floating point overload (TODO)
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

            if (sget_byte(is) == 0xc0)
                return sbump_byte(is) == 0xc0;
            else {
                ref.reset(new Type{});

                return msgpack(*ref).read(is);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_optional_base<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref.reset();

            if (sget_byte(is) == 0xc0)
                return sbump_byte(is) == 0xc0;
            else {
                ref = Type{};

                return msgpack(*ref).read(is);
            }
        }
#endif
    };

    template<typename Type>
    msgpack_writer<Type> msgpack(const Type &, msgpack_options options = {});

    namespace impl {
        namespace msgpack {
            // C++11 doesn't have generic lambdas, so create a functor class that allows writing a tuple
            class write_tuple {
                std::streambuf &os;
                bool &error;
                const msgpack_options &options;

            public:
                constexpr write_tuple(std::streambuf &stream, bool &error, const msgpack_options &options) noexcept
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
        const msgpack_options options;

    public:
        constexpr msgpack_writer(const msgpack_reader<Type> &reader)
            : ref(reader.ref)
            , options(reader.options)
        {}
        constexpr msgpack_writer(const Type &ref, msgpack_options options = {})
            : ref(ref)
            , options(options)
        {}

        // User object overload, skate_msgpack(stream, object, options)
        template<typename _ = Type, typename std::enable_if<type_exists<decltype(skate_msgpack(static_cast<std::streambuf &>(std::declval<std::streambuf &>()), std::declval<const _ &>(), std::declval<msgpack_options>()))>::value &&
                                                            !is_string_base<_>::value &&
                                                            !is_array_base<_>::value &&
                                                            !is_map_base<_>::value &&
                                                            !is_tuple_base<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            // Library user is responsible for creating valid MsgPack in the callback function
            return skate_msgpack(os, ref, options);
        }

        // Array overload (non-binary data)
        template<typename _ = Type,
                 typename Element = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<is_array_base<_>::value &&
                                         !is_tuple_base<_>::value &&
                                         !(std::is_convertible<Element, char>::value &&
                                          (std::is_class<Element>::value || sizeof(Element) == 1)), int>::type = 0>
        bool write(std::streambuf &os) const {
            if (ref.size() <= 15) {
                if (os.sputc(static_cast<unsigned char>(0x90 | ref.size())) == std::char_traits<char>::eof())
                    return false;
            } else if (ref.size() <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdc)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdd)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(ref.size())))
                    return false;
            } else {
                return false;
            }

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (!msgpack(*el, options).write(os))
                    return false;
            }

            return true;
        }

        // Array overload (binary data)
        template<typename _ = Type,
                 typename Element = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<is_array_base<_>::value &&
                                         !is_tuple_base<_>::value &&
                                         std::is_convertible<Element, char>::value &&
                                         (std::is_class<Element>::value || sizeof(Element) == 1), int>::type = 0>
        bool write(std::streambuf &os) const {
            if (ref.size() <= 0xffu) {
                if (os.sputc(static_cast<unsigned char>(0xc4)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint8_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xc5)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xc6)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(ref.size())))
                    return false;
            } else {
                return false;
            }

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (os.sputc(*el) == std::char_traits<char>::eof())
                    return false;
            }

            return true;
        }

        // Tuple/pair overload
        template<typename _ = Type, typename std::enable_if<is_tuple_base<_>::value &&
                                                            !is_array_base<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            constexpr size_t size = std::tuple_size<typename std::decay<_>::type>::value;
            bool error = false;

            if (size <= 15) {
                if (os.sputc(static_cast<unsigned char>(0x90 | size)) == std::char_traits<char>::eof())
                    return false;
            } else if (size <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdc)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(size)))
                    return false;
            } else if (size <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdd)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(size)))
                    return false;
            } else {
                return false;
            }

            impl::apply(impl::msgpack::write_tuple(os, error, options), ref);

            return !error;
        }

        // Map overload
        template<typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            typedef typename std::decay<decltype(begin(ref))>::type KeyValuePair;

            if (ref.size() <= 15) {
                if (os.sputc(static_cast<unsigned char>(0x80 | ref.size())) == std::char_traits<char>::eof())
                    return false;
            } else if (ref.size() <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xde)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdf)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(ref.size())))
                    return false;
            } else {
                return false;
            }

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (!msgpack(key_of<KeyValuePair>{}(el), options).write(os))
                    return false;

                if (!msgpack(value_of<KeyValuePair>{}(el), options).write(os))
                    return false;
            }

            return true;
        }

        // String overload
        template<typename _ = Type,
                 typename StringChar = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<type_exists<decltype(unicode_codepoint(std::declval<StringChar>()))>::value &&
                                         is_string_base<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            const auto s = utf_convert_weak<std::string>(ref);
            const size_t sz = s.get().size();

            if (sz <= 31) {
                if (os.sputc(static_cast<unsigned char>(0xa0 | sz)) == std::char_traits<char>::eof())
                    return false;
            } else if (sz <= 0xffu) {
                if (os.sputc(static_cast<unsigned char>(0xd9)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint8_t(sz)))
                    return false;
            } else if (sz <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xda)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(sz)))
                    return false;
            } else if (sz <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdb)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(sz)))
                    return false;
            } else {
                return false;
            }

            return os.sputn(s.get().c_str(), sz) == sz;
        }

        // Extension overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, msgpack_ext>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            switch (ref.data.size()) {
                case  1: if (os.sputc(static_cast<unsigned char>(0xd4)) == std::char_traits<char>::eof()) return false; break;
                case  2: if (os.sputc(static_cast<unsigned char>(0xd5)) == std::char_traits<char>::eof()) return false; break;
                case  4: if (os.sputc(static_cast<unsigned char>(0xd6)) == std::char_traits<char>::eof()) return false; break;
                case  8: if (os.sputc(static_cast<unsigned char>(0xd7)) == std::char_traits<char>::eof()) return false; break;
                case 16: if (os.sputc(static_cast<unsigned char>(0xd8)) == std::char_traits<char>::eof()) return false; break;
                default:
                    if (ref.data.size() <= 0xffu) {
                        if (os.sputc(static_cast<unsigned char>(0xc7)) == std::char_traits<char>::eof() || impl::write_big_endian(os, uint8_t(ref.data.size())))
                            return false;
                    } else if (ref.data.size() <= 0xffffu) {
                        if (os.sputc(static_cast<unsigned char>(0xc8)) == std::char_traits<char>::eof() || impl::write_big_endian(os, uint16_t(ref.data.size())))
                            return false;
                    } else if (ref.data.size() <= 0xffffffffu) {
                        if (os.sputc(static_cast<unsigned char>(0xc9)) == std::char_traits<char>::eof() || impl::write_big_endian(os, uint16_t(ref.data.size())))
                            return false;
                    } else
                        return false;

                    break;
            }

            return os.sputc(ref.type) != std::char_traits<char>::eof() && os.sputn(ref.data.c_str(), ref.data.size()) == ref.data.size();
        }

        // Null overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            return os.sputc(static_cast<unsigned char>(0xc0)) != std::char_traits<char>::eof();
        }

        // Boolean overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            return os.sputc(static_cast<unsigned char>(0xc2 | ref)) != std::char_traits<char>::eof();
        }

        // Integer overload
        template<typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            if (std::is_signed<_>::value && ref < 0) {
                typename std::make_unsigned<_>::type uref = ref;

                if (ref >= -32)
                    return os.sputc(static_cast<unsigned char>(0xe0 | uref)) != std::char_traits<char>::eof();
                else if (ref >= -0x80)
                    return os.sputc(static_cast<unsigned char>(0xd0)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint8_t(uref));
                else if (ref >= -0x8000l)
                    return os.sputc(static_cast<unsigned char>(0xd1)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint16_t(uref));
                else if (ref >= -0x80000000ll)
                    return os.sputc(static_cast<unsigned char>(0xd2)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint32_t(uref));
                else
                    return os.sputc(static_cast<unsigned char>(0xd3)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint64_t(uref));
            } else if (std::is_signed<_>::value && options.strict_signed_int) { // Positive number, signed
                if (ref <= 0xff)
                    return os.sputc(static_cast<unsigned char>(0xd0)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint8_t(ref));
                else if (ref <= 0xffffl)
                    return os.sputc(static_cast<unsigned char>(0xd1)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint16_t(ref));
                else if (ref <= 0xffffffffll)
                    return os.sputc(static_cast<unsigned char>(0xd2)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint32_t(ref));
                else
                    return os.sputc(static_cast<unsigned char>(0xd3)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint64_t(ref));
            } else { // Positive number, unsigned
                if (ref <= 0x7fu)
                    return os.sputc(static_cast<unsigned char>(ref)) != std::char_traits<char>::eof();
                else if (ref <= 0xffu)
                    return os.sputc(static_cast<unsigned char>(0xcc)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint8_t(ref));
                else if (ref <= 0xffffu)
                    return os.sputc(static_cast<unsigned char>(0xcd)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint16_t(ref));
                else if (ref <= 0xffffffffu)
                    return os.sputc(static_cast<unsigned char>(0xce)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint32_t(ref));
                else
                    return os.sputc(static_cast<unsigned char>(0xcf)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint64_t(ref));
            }
        }

        // Floating point overload
        template<typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            static_assert(std::numeric_limits<_>::is_iec559, "MsgPack doesn't support floating point numbers other than IEEE-754 at the moment");

            if (std::is_same<typename std::decay<float>::type, _>::value || ref == static_cast<_>(float(ref))) { // Is float or fits in float without loss
                uint32_t result = 0;
                memcpy(static_cast<char *>(static_cast<void *>(&result)),
                       static_cast<const char *>(static_cast<const void *>(&ref)),
                       sizeof(result));

                if (os.sputc(static_cast<unsigned char>(0xca)) == std::char_traits<char>::eof())
                    return false;

                return impl::write_big_endian(os, result);
            } else { // Force into double precision
                const double v = ref;
                uint64_t result = 0;
                memcpy(static_cast<char *>(static_cast<void *>(&result)),
                       static_cast<const char *>(static_cast<const void *>(&v)),
                       sizeof(result));

                if (os.sputc(static_cast<unsigned char>(0xcb)) == std::char_traits<char>::eof())
                    return false;

                return impl::write_big_endian(os, result);
            }
        }

        // time_t overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, time_t>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            struct tm tim;

            memset(&tim, 0, sizeof(tim));
            tim.tm_mday = 1;
            tim.tm_year = 70;
            tim.tm_isdst = -1;

            const double diff = difftime(ref, mktime(&tim));

            const int64_t seconds = int64_t(std::floor(diff));
            const uint32_t nanoseconds = uint32_t(std::trunc((diff - std::floor(diff)) * 1000000000));

            if ((seconds & 0xffffffff00000000ull) == 0) {
                if (nanoseconds == 0) { // 4-byte encoding
                    if (os.sputn("\xd6\xff", 2) != 2 ||
                        !impl::write_big_endian(os, uint32_t(seconds)))
                        return false;
                } else { // 8-byte encoding
                    const uint64_t v = seconds | (uint64_t(nanoseconds) << 34);
                    if (os.sputn("\xd7\xff", 2) != 2 ||
                        !impl::write_big_endian(os, v))
                        return false;
                }
            } else { // 12-byte encoding
                if (os.sputn("\xc7\x0c\xff", 3) != 3 ||
                    !impl::write_big_endian(os, nanoseconds) ||
                    !impl::write_big_endian(os, uint64_t(seconds)))
                    return false;
            }

            return true;
        }

        // Smart pointer overload
        template<typename _ = Type, typename std::enable_if<(is_shared_ptr_base<_>::value ||
                                                             is_weak_ptr_base<_>::value ||
                                                             is_unique_ptr_base<_>::value ||
                                                             std::is_pointer<_>::value) && !is_string_base<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            if (!ref) {
                return os.sputc(static_cast<unsigned char>(0xc0)) != std::char_traits<char>::eof();
            } else {
                return msgpack(*ref, options).write(os);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename _ = Type, typename std::enable_if<is_optional_base<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            if (!ref) {
                return os.sputc(static_cast<unsigned char>(0xc0)) != std::char_traits<char>::eof();
            } else {
                return msgpack(*ref, options).write(os);
            }
        }
#endif
    };

    template<typename Type>
    msgpack_reader<Type> msgpack(Type &value, msgpack_options options) { return msgpack_reader<Type>(value, options); }

    template<typename Type>
    msgpack_writer<Type> msgpack(const Type &value, msgpack_options options) { return msgpack_writer<Type>(value, options); }

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
    Type from_msgpack(const std::string &s, bool *error = nullptr) {
        Type value;

        struct one_pass_readbuf : public std::streambuf {
            one_pass_readbuf(const char *buf, size_t size) {
                setg(const_cast<char *>(buf),
                     const_cast<char *>(buf),
                     const_cast<char *>(buf + size));
            }
        } buf{s.c_str(), s.size()};

        if (!msgpack(value).read(buf)) {
            if (error)
                *error = true;

            return {};
        }

        if (error)
            *error = false;

        return value;
    }

    template<typename Type>
    std::string to_msgpack(const Type &value, msgpack_options options = {}) {
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
            d.p = new String(utf_convert_weak<String>(v).get());
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
        S get_string(S default_value = {}) const { return is_string()? utf_convert_weak<S>(unsafe_get_string()).get(): default_value; }
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
            return value(utf_convert_weak<String>(key).get(), default_value);
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
            return (*this)[utf_convert_weak<String>(key).get()];
        }
        template<typename S, typename std::enable_if<is_string_base<S>::value, int>::type = 0>
        basic_msgpack_value &operator[](const S &key) {
            return (*this)[utf_convert_weak<String>(key).get()];
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
            return value(utf_convert_weak<String>(key).get(), default_value);
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
            return (*this)[utf_convert_weak<String>(key).get()];
        }
        template<typename S, typename std::enable_if<is_string_base<S>::value, int>::type = 0>
        basic_msgpack_value<String> &operator[](const S &key) {
            return (*this)[utf_convert_weak<String>(key).get()];
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
            case std::char_traits<char>::eof(): return false;
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
    bool skate_msgpack(std::basic_streambuf<StreamChar> &os, const basic_msgpack_value<String> &j, msgpack_options options) {
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
